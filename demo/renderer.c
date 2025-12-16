#include <SDL2/SDL.h>
#include <SDL2/SDL_opengl.h>
#include <assert.h>
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include "renderer.h"
#include "atlas.inl"

#define BUFFER_SIZE 16384

typedef struct
{
  float pos[2];
  float uv[2];
  unsigned char color[4];
} Vertex;

static Vertex vert_buf[BUFFER_SIZE * 4];
static GLuint index_buf[BUFFER_SIZE * 6];

static int width = 800;
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

static GLuint compile_shader(GLenum type, const char *src)
{
  GLuint shader = glCreateShader(type);
  glShaderSource(shader, 1, &src, NULL);
  glCompileShader(shader);

  GLint status;
  glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
  if (status == GL_FALSE)
  {
    char log[1024];
    glGetShaderInfoLog(shader, sizeof(log), NULL, log);
    fprintf(stderr, "Shader compile error: %s\n", log);
    exit(1);
  }
  return shader;
}

void r_init(void)
{
  /* init SDL window with Core Profile */
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
  SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);

  window = SDL_CreateWindow(
      NULL, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED,
      width, height, SDL_WINDOW_OPENGL);
  SDL_GL_CreateContext(window);

  /* init gl */
  /*
  1. 混合 (Blending)
   1 glEnable(GL_BLEND);
   2 glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
   * 目的: 实现半透明效果。
   * 解释:
       * glEnable(GL_BLEND): 开启混合功能。如果不开启，新绘制的像素会直接覆盖掉旧的像素，不管它是否透明。
       * glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA): 定义混合公式。
           * 这是一个标准的“Alpha 混合”公式。
           * 源因子 (`GL_SRC_ALPHA`): 取新绘制颜色（源颜色）的 Alpha 值作为权重。
           * 目标因子 (`GL_ONE_MINUS_SRC_ALPHA`): 取 1 - 源Alpha 作为背景颜色（目标颜色）的权重。
           * 结果: 最终颜色 = (源颜色 * Alpha) + (背景颜色 * (1 - Alpha))。
       * 在 UI 中的作用: UI 经常有半透明的窗口背景、阴影、圆角或者抗锯齿的字体边缘，这些都需要 Alpha 混合才能正确显示。

  2. 面剔除 (Culling)
   1 glDisable(GL_CULL_FACE);
   * 目的: 允许绘制所有朝向的面，或者说是为了简化 2D 绘制逻辑。
   * 解释:
       * glEnable(GL_CULL_FACE) 通常用于 3D 渲染，为了优化性能，会剔除（不渲染）背对摄像机的三角形（根据顶点顺时针/逆时针顺序判断）。
       * 在 UI 中的作用: 在 2D UI
         渲染中，我们通常只关心平面上的矩形，而且可能会有翻转（Flip）操作导致顶点顺序改变。禁用面剔除可以保证无论顶点的缠绕顺序如何，或者无论矩形是否被翻转，它都能被显示出来。虽然对于严格规范的 2D
         引擎也可以开启剔除，但禁用它能减少因顶点顺序错误导致的“消失”问题，更加稳健。

  3. 深度测试 (Depth Test)
   1 glDisable(GL_DEPTH_TEST);
   * 目的: 按照绘制顺序覆盖像素，而不是按照深度（Z轴距离）。
   * 解释:
       * glEnable(GL_DEPTH_TEST) 用于 3D 渲染，确保近处的物体遮挡远处的物体，通过 Z-Buffer 实现。
       * 在 UI 中的作用: 2D UI 的渲染顺序通常由代码的执行顺序决定（“画家算法”）。后绘制的控件（如弹出的菜单、悬浮窗）应该覆盖在先绘制的控件（如背景、底层窗口）之上。如果你开启了深度测试，除非你手动管理每个 UI 元素的
         Z 坐标，否则可能会出现渲染伪影或错误的遮挡关系。禁用深度测试，直接让后绘制的像素覆盖前面的像素，完全符合 UI 的层级逻辑。

  4. 裁剪测试 (Scissor Test)
   1 glEnable(GL_SCISSOR_TEST);
   * 目的: 限制绘制区域。
   * 解释:
       * 开启裁剪测试后，只有在 glScissor 指定的矩形区域内的像素才会被修改，区域外的绘制会被忽略。
       * 在 UI 中的作用: 这是实现 UI 裁剪功能的关键。例如：
           * 滚动区域: 当你在一个固定大小的窗口中滚动内容时，超出窗口边界的内容不应该显示出来。
           * 窗口边界: 子控件不应该画到父窗口的外面。
       * MicroUI 广泛使用 r_set_clip_rect（内部调用 glScissor）来确保控件只在允许的区域内渲染。
  */
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
  if (status == GL_FALSE)
  {
    char log[1024];
    glGetProgramInfoLog(prog, sizeof(log), NULL, log);
    fprintf(stderr, "Program link error: %s\n", log);
    exit(1);
  }

  unif_tex = glGetUniformLocation(prog, "tex_sampler");
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
  glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, pos));
  /* 1: uv (2 floats) */
  glEnableVertexAttribArray(1);
  glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)offsetof(Vertex, uv));
  /* 2: color (4 ubytes, normalized) */
  glEnableVertexAttribArray(2);
  glVertexAttribPointer(2, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void *)offsetof(Vertex, color));

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

static void flush(void)
{
  if (buf_idx == 0)
  {
    return;
  }

  glUseProgram(prog);

  /* projection matrix */
  float proj[4][4] = {
      {2.0f / width, 0.0f, 0.0f, 0.0f},
      {0.0f, -2.0f / height, 0.0f, 0.0f},
      {0.0f, 0.0f, -1.0f, 0.0f},
      {-1.0f, 1.0f, 0.0f, 1.0f}};
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

static void push_quad(mu_Rect dst, mu_Rect src, mu_Color color)
{
  if (buf_idx == BUFFER_SIZE)
  {
    flush();
  }

  int element_idx = buf_idx * 4; // 当前 Quad 第一个顶点的索引号 (因为每个 Quad 有 4 个顶点)
  int index_idx = buf_idx * 6;   // 当前 Quad 在索引缓冲区里的起始位置 (因为每个 Quad 需要 6 个索引来定义 2 个三角形)

  /* tex coords */
  // 输入参数 src 是纹理图集（Atlas）中的像素坐标。
  // OpenGL 使用 0.0 到 1.0 的归一化坐标。这里将像素坐标除以图集的总宽高 (ATLAS_WIDTH, ATLAS_HEIGHT) 转换为 UV 坐标。
  float x = src.x / (float)ATLAS_WIDTH;
  float y = src.y / (float)ATLAS_HEIGHT;
  float w = src.w / (float)ATLAS_WIDTH;
  float h = src.h / (float)ATLAS_HEIGHT;

  /* vertices */
  int v_idx = buf_idx * 4;
  /*
   * 填充 4 个顶点的数据（左上、右上、左下、右下）。每个顶点包含：
   * Position (`pos`): 屏幕上的像素坐标。
   * UV (`uv`): 刚刚计算出的纹理坐标。
   * Color (`color`): 顶点的颜色（RGBA）。
   */
  /* 0: Top-Left */
  vert_buf[v_idx + 0].pos[0] = dst.x;
  vert_buf[v_idx + 0].pos[1] = dst.y;
  vert_buf[v_idx + 0].uv[0] = x;
  vert_buf[v_idx + 0].uv[1] = y;
  memcpy(vert_buf[v_idx + 0].color, &color, 4);

  /* 1: Top-Right */
  vert_buf[v_idx + 1].pos[0] = dst.x + dst.w;
  vert_buf[v_idx + 1].pos[1] = dst.y;
  vert_buf[v_idx + 1].uv[0] = x + w;
  vert_buf[v_idx + 1].uv[1] = y;
  memcpy(vert_buf[v_idx + 1].color, &color, 4);

  /* 2: Bottom-Left */
  vert_buf[v_idx + 2].pos[0] = dst.x;
  vert_buf[v_idx + 2].pos[1] = dst.y + dst.h;
  vert_buf[v_idx + 2].uv[0] = x;
  vert_buf[v_idx + 2].uv[1] = y + h;
  memcpy(vert_buf[v_idx + 2].color, &color, 4);

  /* 3: Bottom-Right */
  vert_buf[v_idx + 3].pos[0] = dst.x + dst.w;
  vert_buf[v_idx + 3].pos[1] = dst.y + dst.h;
  vert_buf[v_idx + 3].uv[0] = x + w;
  vert_buf[v_idx + 3].uv[1] = y + h;
  memcpy(vert_buf[v_idx + 3].color, &color, 4);

  /* indices */
  // 填充 6 个索引，定义 2 个三角形。每个索引指向 `vert_buf` 中的一个顶点。
  // 第一个三角形：0 -> 1 -> 2 (左上 -> 右上 -> 左下)
  // 第二个三角形：2 -> 3 -> 1 (左下 -> 右下 -> 右上)
  index_buf[index_idx + 0] = element_idx + 0;
  index_buf[index_idx + 1] = element_idx + 1;
  index_buf[index_idx + 2] = element_idx + 2;
  index_buf[index_idx + 3] = element_idx + 2;
  index_buf[index_idx + 4] = element_idx + 3;
  index_buf[index_idx + 5] = element_idx + 1;

  buf_idx++;
}

void r_draw_rect(mu_Rect rect, mu_Color color)
{
  push_quad(rect, atlas[ATLAS_WHITE], color);
}

void r_draw_text(const char *text, mu_Vec2 pos, mu_Color color)
{
  mu_Rect dst = {pos.x, pos.y, 0, 0};
  for (const char *p = text; *p; p++)
  {
    if ((*p & 0xc0) == 0x80)
    {
      continue;
    }
    int chr = mu_min((unsigned char)*p, 127);
    mu_Rect src = atlas[ATLAS_FONT + chr];
    dst.w = src.w;
    dst.h = src.h;
    push_quad(dst, src, color);
    dst.x += dst.w;
  }
}

void r_draw_icon(int id, mu_Rect rect, mu_Color color)
{
  mu_Rect src = atlas[id];
  int x = rect.x + (rect.w - src.w) / 2;
  int y = rect.y + (rect.h - src.h) / 2;
  push_quad(mu_rect(x, y, src.w, src.h), src, color);
}

/*
 * 参数:
 * text: 指向 C 字符串的指针，表示要计算宽度的文本。
 * len: 要处理的文本的最大长度。这允许你计算部分字符串的宽度。
 * 功能: 它遍历文本中的每个字符，查找该字符在 atlas（纹理图集）中对应的宽度，并将这些宽度累加起来。对于多字节 UTF-8 字符（尽管这里简单地跳过，只处理 ASCII 字符），它会跳过后续字节。
 * 返回值: 文本的总像素宽度。
 */
int r_get_text_width(const char *text, int len)
{
  int res = 0;
  // 遍历文本中的每个字符，计算其在纹理图集中的宽度并累加
  for (const char *p = text; *p && len--; p++)
  {
    // 跳过 UTF-8 编码的多字节字符的后续字节（这里假设只处理 ASCII 字符或单字节字符）
    if ((*p & 0xc0) == 0x80)
    {
      continue;
    }
    // 获取字符的 ASCII 值，并限制在 0-127 范围内，以对应图集中的字体索引
    int chr = mu_min((unsigned char)*p, 127);
    // 累加字符在图集中的宽度
    res += atlas[ATLAS_FONT + chr].w;
  }
  return res;
}

/*
这个函数返回文本的固定行高。

* 功能: 在这个渲染器中，字体的高度是预设的固定值 18 像素。这个函数直接返回这个值，而不是根据实际字符计算高度，因为 MicroUI 库中的文本通常是单行且固定高度的。
* 返回值: 文本的固定像素高度。

*/
int r_get_text_height(void)
{
  // 返回文本的固定像素高度。在当前渲染器中，字体高度是预设的固定值。
  return 18;
}

/*
这个函数用于设置 OpenGL 的裁剪矩形（Scissor Rectangle）。裁剪矩形定义了屏幕上的一个区域，只有在这个区域内的像素才会被渲染，区域外的像素会被丢弃。

* 参数:
    * rect: mu_Rect 结构体，定义了裁剪区域的 x, y 坐标，以及 width 和 height。
* 功能:
    1. 调用 flush(): 在改变裁剪区域之前，它会先强制渲染所有当前批处理缓冲区中的几何体。这是因为裁剪设置会影响后续的所有绘制操作，所以需要确保在旧的裁剪设置下绘制完所有 pending 的内容。
    2. 调用 glScissor(): 这是 OpenGL 函数，用于设置裁剪矩形。
        * rect.x, rect.y: 裁剪矩形的左下角坐标。注意: OpenGL 的 glScissor 函数的 y 坐标是从屏幕底部开始计算的，而 MicroUI 的坐标系通常是从左上角开始的。所以这里需要进行转换：height - (rect.y + rect.h) 将 MicroUI
          的 y 和 height 转换为 OpenGL 的底部起始 y 坐标。
        * rect.w, rect.h: 裁剪矩形的宽度和高度。
*/
void r_set_clip_rect(mu_Rect rect)
{
  flush();
  glScissor(rect.x, height - (rect.y + rect.h), rect.w, rect.h);
}

void r_clear(mu_Color clr)
{
  flush();
  glClearColor(clr.r / 255., clr.g / 255., clr.b / 255., clr.a / 255.);
  glClear(GL_COLOR_BUFFER_BIT);
}

void r_present(void)
{
  flush();
  SDL_GL_SwapWindow(window);
}