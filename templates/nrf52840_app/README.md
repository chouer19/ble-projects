# nRF52840 应用模板

从本目录复制到 `projects/<your_app>/` 开始新项目：

```bash
cp -r templates/nrf52840_app projects/my_app
# 编辑 CMakeLists.txt 中的 project() 名称
cd projects/my_app
make build
make flash-direct
```
