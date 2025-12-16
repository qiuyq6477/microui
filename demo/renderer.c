#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "renderer.h"
#include "atlas.inl"

#define BUFFER_SIZE 16384

typedef struct {
  float pos[2];
  float uv[2];
  unsigned char color[4];
} Vertex;

static Vertex  vert_buf[BUFFER_SIZE * 4];
static GLuint index_buf[BUFFER_SIZE * 6];

static int width  = 800;
static int height = 600;
static int buf_idx;

static SDL_Window *window;

static GLuint prog;
static GLuint tex;
static GLint unif_tex;
static GLint unif_proj;
static GLuint vao, vbo, ebo;

static const char *vert_shader_src =
  "#version 330 core\n"
  "layout(location = 0) in vec2 in_pos;\n"
  "layout(location = 1) in vec2 in_uv;\n"
  "layout(location = 2) in vec4 in_color;\n"
  "uniform mat4 proj;\n"
  "out vec2 ex_uv;\n"
  "out vec4 ex_color;\n"
  "void main() {\n"
  "  gl_Position = proj * vec4(in_pos, 0.0, 1.0);\n"
  "  ex_uv = in_uv;\n"
  "  ex_color = in_color;\n"
  "}\n";

static const char *frag_shader_src =
  "#version 330 core\n"
  "in vec2 ex_uv;\n"
  "in vec4 ex_color;\n"
  "uniform sampler2D tex_sampler;\n"
  "out vec4 out_color;\n"
  "void main() {\n"
  "  out_color = texture(tex_sampler, ex_uv) * ex_color;\n"
  "}\n";


static GLuint compile_shader(GLenum type, const char *src) {
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &src, NULL);
  glCompileShader(shader);

  GLint status;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
  if (status == GL_FALSE) {
    char log[1024];
    glGetShaderInfoLog(shader, sizeof(log), NULL, log);
    fprintf(stderr, "Shader compile error: %s\n", log);
    exit(1);
  }
  return shader;
}


void r_init(void) {
  /* init SDL window with Core Profile */
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

  window = SDL_CreateWindow(
    NULL, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
    width, height, SDL_WINDOW_OPENGL);
  SDL_GL_CreateContext(window);

  /* init gl */
  glEnable(GL_BLEND);
  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
  glDisable(GL_CULL_FACE);
  glDisable(GL_DEPTH_TEST);
  glEnable(GL_SCISSOR_TEST);

  /* init shaders */
  GLuint vert = compile_shader(GL_VERTEX_SHADER, vert_shader_src);
  GLuint frag = compile_shader(GL_FRAGMENT_SHADER, frag_shader_src);
  prog = glCreateProgram();
  glAttachShader(prog, vert);
  glAttachShader(prog, frag);
  glLinkProgram(prog);

  GLint status;
  glGetProgramiv(prog, GL_LINK_STATUS, &status);
  if (status == GL_FALSE) {
     char log[1024];
     glGetProgramInfoLog(prog, sizeof(log), NULL, log);
     fprintf(stderr, "Program link error: %s\n", log);
     exit(1);
  }

  unif_tex  = glGetUniformLocation(prog, "tex_sampler");
  unif_proj = glGetUniformLocation(prog, "proj");

  /* init buffers */
  glGenVertexArrays(1, &vao);
  glBindVertexArray(vao);

  glGenBuffers(1, &vbo);
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vert_buf), NULL, GL_DYNAMIC_DRAW);

  glGenBuffers(1, &ebo);
  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);

  /* Attributes */
  /* 0: pos (2 floats) */
  glEnableVertexAttribArray(0);
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, pos));
  /* 1: uv (2 floats) */
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, uv));
  /* 2: color (4 ubytes, normalized) */
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void*)offsetof(Vertex, color));

  /* init texture */
  glGenTextures(1, &tex);
  glBindTexture(GL_TEXTURE_2D, tex);
  /* Use GL_RED and Swizzle to emulate GL_ALPHA (1,1,1,R) */
  GLint swizzleMask[] = {GL_ONE, GL_ONE, GL_ONE, GL_RED};
  glTexParameteriv(GL_TEXTURE_2D, GL_TEXTURE_SWIZZLE_RGBA, swizzleMask);

  glTexImage2D(GL_TEXTURE_2D, 0, GL_RED, ATLAS_WIDTH, ATLAS_HEIGHT, 0,
    GL_RED, GL_UNSIGNED_BYTE, atlas_texture);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
}


static void flush(void) {
  if (buf_idx == 0) { return; }

  glUseProgram(prog);

  /* projection matrix */
  float proj[4][4] = {
    { 2.0f/width, 0.0f,        0.0f, 0.0f },
    { 0.0f,      -2.0f/height, 0.0f, 0.0f },
    { 0.0f,       0.0f,       -1.0f, 0.0f },
    { -1.0f,      1.0f,        0.0f, 1.0f }
  };
  glUniformMatrix4fv(unif_proj, 1, GL_FALSE, &proj[0][0]);

  glUniform1i(unif_tex, 0);

  glBindVertexArray(vao);

  /* Update buffers */
  glBindBuffer(GL_ARRAY_BUFFER, vbo);
  glBufferSubData(GL_ARRAY_BUFFER, 0, buf_idx * 4 * sizeof(Vertex), vert_buf);

  glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
  glBufferData(GL_ELEMENT_ARRAY_BUFFER, buf_idx * 6 * sizeof(GLuint), index_buf, GL_STREAM_DRAW);

  glDrawElements(GL_TRIANGLES, buf_idx * 6, GL_UNSIGNED_INT, 0);

  buf_idx = 0;
}


static void push_quad(mu_Rect dst, mu_Rect src, mu_Color color) {
  if (buf_idx == BUFFER_SIZE) { flush(); }

  int element_idx = buf_idx * 4;
  int index_idx   = buf_idx * 6;

  /* tex coords */
  float x = src.x / (float) ATLAS_WIDTH;
  float y = src.y / (float) ATLAS_HEIGHT;
  float w = src.w / (float) ATLAS_WIDTH;
  float h = src.h / (float) ATLAS_HEIGHT;

  /* vertices */
  int v_idx = buf_idx * 4;

  /* 0: Top-Left */
  vert_buf[v_idx+0].pos[0] = dst.x;
  vert_buf[v_idx+0].pos[1] = dst.y;
  vert_buf[v_idx+0].uv[0]  = x;
  vert_buf[v_idx+0].uv[1]  = y;
  memcpy(vert_buf[v_idx+0].color, &color, 4);

  /* 1: Top-Right */
  vert_buf[v_idx+1].pos[0] = dst.x + dst.w;
  vert_buf[v_idx+1].pos[1] = dst.y;
  vert_buf[v_idx+1].uv[0]  = x + w;
  vert_buf[v_idx+1].uv[1]  = y;
  memcpy(vert_buf[v_idx+1].color, &color, 4);

  /* 2: Bottom-Left */
  vert_buf[v_idx+2].pos[0] = dst.x;
  vert_buf[v_idx+2].pos[1] = dst.y + dst.h;
  vert_buf[v_idx+2].uv[0]  = x;
  vert_buf[v_idx+2].uv[1]  = y + h;
  memcpy(vert_buf[v_idx+2].color, &color, 4);

  /* 3: Bottom-Right */
  vert_buf[v_idx+3].pos[0] = dst.x + dst.w;
  vert_buf[v_idx+3].pos[1] = dst.y + dst.h;
  vert_buf[v_idx+3].uv[0]  = x + w;
  vert_buf[v_idx+3].uv[1]  = y + h;
  memcpy(vert_buf[v_idx+3].color, &color, 4);

  /* indices */
  index_buf[index_idx + 0] = element_idx + 0;
  index_buf[index_idx + 1] = element_idx + 1;
  index_buf[index_idx + 2] = element_idx + 2;
  index_buf[index_idx + 3] = element_idx + 2;
  index_buf[index_idx + 4] = element_idx + 3;
  index_buf[index_idx + 5] = element_idx + 1;

  buf_idx++;
}


void r_draw_rect(mu_Rect rect, mu_Color color) {
  push_quad(rect, atlas[ATLAS_WHITE], color);
}


void r_draw_text(const char *text, mu_Vec2 pos, mu_Color color) {
  mu_Rect dst = { pos.x, pos.y, 0, 0 };
  for (const char *p = text; *p; p++) {
    if ((*p & 0xc0) == 0x80) { continue; }
    int chr = mu_min((unsigned char) *p, 127);
    mu_Rect src = atlas[ATLAS_FONT + chr];
    dst.w = src.w;
    dst.h = src.h;
    push_quad(dst, src, color);
    dst.x += dst.w;
  }
}


void r_draw_icon(int id, mu_Rect rect, mu_Color color) {
  mu_Rect src = atlas[id];
  int x = rect.x + (rect.w - src.w) / 2;
  int y = rect.y + (rect.h - src.h) / 2;
  push_quad(mu_rect(x, y, src.w, src.h), src, color);
}


int r_get_text_width(const char *text, int len) {
  int res = 0;
  for (const char *p = text; *p && len--; p++) {
    if ((*p & 0xc0) == 0x80) { continue; }
    int chr = mu_min((unsigned char) *p, 127);
    res += atlas[ATLAS_FONT + chr].w;
  }
  return res;
}


int r_get_text_height(void) {
  return 18;
}


void r_set_clip_rect(mu_Rect rect) {
  flush();
  glScissor(rect.x, height - (rect.y + rect.h), rect.w, rect.h);
}


void r_clear(mu_Color clr) {
  flush();
  glClearColor(clr.r / 255., clr.g / 255., clr.b / 255., clr.a / 255.);
  glClear(GL_COLOR_BUFFER_BIT);
}


void r_present(void) {
  flush();
  SDL_GL_SwapWindow(window);
}