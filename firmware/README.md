# Quellog Firmware

`firmware/` 是 Quellog 墨水屏终端的 ESP-IDF 子工程。

当前版本只搭建固件框架：

- 默认板型：`Zectrix S3 e-paper 4.2"`
- 分层：`Board` / `Display` / `Application` / `UiPage`
- 首页：账本总览
- 页面：总览、最近记录、统计、设置
- 数据：本地占位数据

## 目录

- `main/application.*`：应用状态机与页面切换
- `main/display/*`：显示抽象、页面模型、页面注册
- `main/boards/*`：板级抽象与默认板型实现
- `main/settings.*`：NVS 配置读写

## 构建

```bash
cd firmware
./build.sh
```

如果未加载 ESP-IDF 环境，请先执行对应版本的 `export.sh`。
