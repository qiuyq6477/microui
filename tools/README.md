# Atlas tools (demo)

提供用于导出与打包纹理图集的小脚本（依赖 Pillow）:

1. **extract_visualize_atlas.py**
   - 用途: 从 `atlas.inl` 提取单通道纹理并保存为 `atlas_extracted.png`（灰度）和 `atlas_extracted_rgba.png`（白色 RGB + alpha），并按 `atlas[]` 裁切出子图到 `demo/atlas_parts/`。
   - 用法:
     ```powershell
     python .\demo\extract_visualize_atlas.py
     # 或者对其他 .inl 文件
     python .\demo\extract_visualize_atlas.py --infile atlas_new.inl
     ```

2. **export_atlas_metadata.py**
   - 用途: 从 `atlas.inl` 导出元数据到 `atlas_metadata.json`，并生成一个简单的 `atlas.fnt`（可用于一些位图字体工具）。
   - 用法:
     ```powershell
     python .\demo\export_atlas_metadata.py
     ```

3. **pack_atlas.py**
   - 用途: 使用单通道图像（灰度或 RGBA 的 alpha）和 `atlas_metadata.json` 重新生成 `atlas.inl` 格式文件（例如 `atlas_new.inl`）。
   - 用法:
     ```powershell
     python .\demo\pack_atlas.py --image .\demo\atlas_extracted.png --meta .\demo\atlas_metadata.json --out .\demo\atlas_new.inl
     ```

测试流程（示例）:
```powershell
pip install --user pillow
python .\demo\extract_visualize_atlas.py
python .\demo\export_atlas_metadata.py
python .\demo\pack_atlas.py --image .\demo\atlas_extracted.png --meta .\demo\atlas_metadata.json --out .\demo\atlas_new.inl
python .\demo\extract_visualize_atlas.py --infile atlas_new.inl
```

如果需要，我可以把 `atlas_new.inl` 覆盖回 `atlas.inl` 并提交到仓库，或把 `.fnt` 转换为更完整的 BMFont 格式。