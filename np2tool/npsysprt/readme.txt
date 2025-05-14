■■■ Neko Project II システムポートドライバ for WinNT ■■■

Windows NT系からNeko Project IIのシステムポートにアクセスするためのデバイスドライバです。
エミュレータ内の仮想環境からNeko Project IIと通信できます。
Neko Project 21/Wの場合、動的CPUクロック変更などが行えます。

●動作環境
Windows NT4.0, 2000


●インストール
レガシードライバ扱いでインストールします。
npsysprt.infを右クリック→インストール
でインストールできます。
アップデートも同じ操作でできますが、反映にはシステムの再起動が必要です。
ほか、付属のregファイルをインポートしてもインストールできます。


●アンインストール
・WinNT4.0の場合
コントロールパネル→デバイスでNeko Project II System Portのスタートアップを無効にしてください。
完璧に消したい場合、レジストリエディタで
HKLM\System\CurrentControlSet\Services\npsysprt
を消してください。
ファイルも消したい場合は
System32\drivers\npsysprt.sys
を消してください。

・Win2000の場合
デバイスマネージャ→表示→非表示のデバイスの表示
でプラグアンドプレイではないドライバを表示させ、
Neko Project II System Portを削除してください。


●使い方
npcngclk.exeはNeko Project 21/Wの動的CPUクロック変更のためのツールです。
npcngclk <変更したいクロック倍率>
で、動的にCPUクロック倍率が変わります。この変更はEmulate→Resetで元に戻ります。

npsyscmd.exeはNeko Project IIのシステムポートに任意のコマンドを送信できるツールです。
npsyscmd <コマンド> [引数byte1] [引数byte2] [引数byte3] [引数byte4]
で送信できます。なんでも送れてしまうので要注意（例えばpoweroffとかを送るとnp2が終了します）。

例えば、
npsyscmd NP2
を実行すると、NP2と返ってきます。

送信できるコマンドはNeko Project IIのソースコードや以下のページに記載があります。
https://simk98.github.io/np21w/docs/npcngcfg.html


●ソースコード
np21/wソースのnp2tool\npsysprtにソースコードがあります。
いちおう猫本体と同じ修正BSDライセンスとしますが、実質的に自由に使っていただいて大丈夫です。


------------------------
Neko Project 21/W 開発者
SimK
