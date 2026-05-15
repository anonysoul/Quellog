# Quellog E-Ink Firmware

`firmware/` 是 Quellog 墨水屏客户端固件子工程。

当前版本提供墨水屏客户端固件骨架：

- 默认板型：`Zectrix S3 e-paper 4.2"`
- 分层：`Board` / `Display` / `Application` / `UiPage`
- 首页：账本总览
- 页面：总览、最近记录、统计、设置
- 数据：本地占位数据
- 中文显示：内置 UTF-8 文本渲染与独立字体分区

## 目录

- `main/application.*`：应用状态机、设备状态与页面切换
- `main/display/*`：显示抽象、页面模型、页面注册
- `main/boards/*`：板级抽象与默认板型实现
- `main/settings.*`：NVS 配置读写
- `assets/LXGWWenKai-Regular.ttf`：霞鹜文楷字体源文件
- `assets/font_partition.bin`：基于霞鹜文楷预生成的中文字库分区镜像
- `scripts/generate_font_partition.py`：字库维护脚本

## 构建

```bash
cd firmware
./build.sh
```

如果未加载 ESP-IDF 环境，请先执行对应版本的 `export.sh`。

## Docker 构建

如果本机未安装或未加载 ESP-IDF 环境，也可以直接使用官方 Docker 镜像构建。

在仓库根目录执行：

```bash
docker run --rm \
  -v "$(pwd)/firmware:/project" \
  -w /project \
  espressif/idf:release-v5.2 \
  bash -lc 'idf.py set-target esp32s3 && idf.py build'
```

构建产物默认输出到 `firmware/build/` 目录。

说明：

- 正常 `idf.py build` 会生成应用镜像与 `build/font_partition.bin`。
- 默认 `idf.py flash` 仍只烧录 bootloader / partition-table / app。
- 若需要完整中文显示，请使用 Release 包中的 `flash_args` 或 `merged-binary.bin` 进行烧录。

## Release 打包

本地打包命令：

```bash
cd firmware
python3 scripts/build_release.py
```

脚本会：

- 读取 `CMakeLists.txt` 中的 `PROJECT_VER`
- 构建 `esp32s3 / zectrix-s3-epaper-4.2`
- 执行 `idf.py merge-bin`
- 输出 `dist/release/v<version>/quellog-firmware_v<version>.zip`

压缩包内包含：

- `merged-binary.bin`
- `flash_args`
- `flasher_args.json`
- `bootloader.bin`
- `partition-table.bin`
- `quellog_firmware.bin`
- `manifest.json`
- `FLASHING.md`

版本号统一在 `firmware/CMakeLists.txt` 的 `PROJECT_VER` 修改。

## 烧录 Release 包

从 GitHub Release 下载并解压 zip 后，可使用以下两种方式烧录。

多文件烧录：

```bash
python -m esptool --chip esp32s3 -p <PORT> -b 460800 --before default_reset --after hard_reset write_flash @flash_args
```

单文件烧录：

```bash
python -m esptool --chip esp32s3 -p <PORT> -b 460800 --before default_reset --after hard_reset write_flash 0x0 merged-binary.bin
```
