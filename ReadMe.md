# NCMCacheTransfer

## 网易云音乐缓存文件转换工具

将网易云缓存文件（Windows端为`*.uc`，Android端为`*.uc!`）转换为普通媒体文件。

Windows客户端在设置中查看缓存路径，Android客户端缓存文件位于 `内部存储/netease/cloudmusic/Cache/Music1` 。

项目使用Qt 6.2版本，目前发现QMediaPlayer无法播放FLAC等无损格式的音乐文件。
