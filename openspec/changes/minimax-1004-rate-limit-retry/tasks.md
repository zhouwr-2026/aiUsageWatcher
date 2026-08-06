# MiniMax KWallet 命名修复热修 — Tasks

## Task 1: 修正 KWallet folder 常量
- [ ] `src/minimaxclient.cpp` wallet folder 改为 `AIQuotaPilot`
- [ ] `src/codexzhclient.cpp` wallet folder 改为 `AIQuotaPilot`
- [ ] 不读取、不迁移、不删除旧 `AI Usage Watcher` entry

## Task 2: 验证旧名不再进入运行时代码
- [ ] `rg "AI Usage Watcher" src` 不再命中
- [ ] `rg "AI Usage Watcher" package` 不命中

## Task 3: 构建 + C++ 测试
- [ ] CMake 重新构建
- [ ] `ctest -R 'minimax|codex'` 通过

## Task 4: 部署 + 本机验证
- [ ] `cmake --install` 安装新插件
- [ ] 重启 `plasmashell`
- [ ] 只列 `AIQuotaPilot` wallet entry 名称，不读取密码
- [ ] 在配置页重新保存 MiniMax Key 后刷新面板

## Task 5: 对抗性自审 + verify
- [ ] `git diff --stat` 确认范围只包含 KWallet 命名与本 change 文档
- [ ] 确认没有 API Key 出现在 diff / 日志 / 文档
- [ ] 写 verification report
- [ ] Comet verify / archive
