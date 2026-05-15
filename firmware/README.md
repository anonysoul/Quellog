# Quellog E-Ink Firmware

`firmware/` 是 Quellog 墨水屏客户端固件子工程。

当前版本提供墨水屏客户端固件骨架：

- 默认板型：`Zectrix S3 e-paper 4.2"`
- 分层：`Board` / `Display` / `Application` / `UiPage`
- 首页：账本总览
- 页面：总览、最近记录、统计、设置
- 数据：本地占位数据

## 目录

- `main/application.*`：应用状态机、设备状态与页面切换
- `main/display/*`：显示抽象、页面模型、页面注册
- `main/boards/*`：板级抽象与默认板型实现
- `main/settings.*`：NVS 配置读写

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
