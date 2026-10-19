# ==============================================================================
# MotoNav 保持上游同步与分支维护脚本
# ==============================================================================
param(
    [string]$UpstreamUrl = "https://github.com/zyn950929/MotoNav.git",
    [string]$TargetBranch = "feat/board-m5stopwatch"
)

$ErrorActionPreference = "Stop"

Write-Host ">>> 检查 git 远程仓库配置..." -ForegroundColor Cyan
$remotes = git remote -v
if ($remotes -notmatch "upstream") {
    Write-Host ">>> 添加 upstream 远程源: $UpstreamUrl" -ForegroundColor Green
    git remote add upstream $UpstreamUrl
}

Write-Host ">>> 获取上游 upstream 最新提交和分支..." -ForegroundColor Cyan
git fetch upstream

Write-Host ">>> 同步本地 main 分支到 upstream/main..." -ForegroundColor Cyan
git checkout main
git merge upstream/main --ff-only
Write-Host ">>> 推送更新后的 main 分支到你的 origin 仓库..." -ForegroundColor Cyan
git push origin main

Write-Host ">>> 切换回特性开发分支 $TargetBranch 并进行 rebase..." -ForegroundColor Cyan
git checkout $TargetBranch
git rebase main

Write-Host "=================================================================" -ForegroundColor Green
Write-Host "同步完成！当前 $TargetBranch 分支已成功合入上游最新特性。" -ForegroundColor Green
Write-Host "如有推送需要，请使用: git push origin $TargetBranch --force-with-lease" -ForegroundColor Yellow
Write-Host "=================================================================" -ForegroundColor Green
