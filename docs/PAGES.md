# GitHub Pages 在线试玩

公开地址：https://svenkunkka.github.io/emberfall-chromatic/

Pages 发布源为 `main` 分支 `/docs`，`.nojekyll` 保证静态文件直接发布。

此目录中的 `js/binjgb.js`、`js/binjgb.wasm`、`js/script.js` 和 `css/style.css` 复用已验收的 GB Studio 官方浏览器导出。`js/README.md` 保留模拟器许可说明。游戏 ROM 与 `release-manifest.json` 的 `web_rom.sha256` 相符。没有改变原生游戏代码或 ROM。

`index.html`、`css/pages.css` 和 `js/pages.js` 提供公开页面的操作提示、手机布局和普通按键入口。`gbstudio.json` 额外为 Select 保留 P 键，与项目 README 一致。

后续更新：先重新构建并验收正式浏览器导出，再更新上述运行时、配置和 `rom/`；保留公开页面外壳。提交并推送 `main` 后，GitHub 自动发布 `/docs`。部署异常时可撤销对应发布提交并推送，恢复上一版静态文件。
