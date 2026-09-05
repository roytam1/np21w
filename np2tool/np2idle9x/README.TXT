■■■ Neko Project II HLT IDLEドライバ for Win9x ■■■

Neko Project II上のWin9xでアイドル時にHLT命令を呼ぶVxDドライバです。
Win9x現役時代にあったRain 2.0やCPUIdle等のCPU冷却系プログラムに相当します。

ただし、これら現役時代のソフトと比較するとかなり簡易的な処理になっています。
実CPUで実行したときどうなるかは検証できていません。エミュレータ専用です。


●動作環境
【ゲスト】
Windows 95, 98

【ホスト】
Neko Project 21/W


●インストール
ドライバをゲストOSに登録して使用します。
SETUP.EXEを実行して画面の指示に従えばインストールできます。


●アンインストール
SETUP.EXEを再実行してアンインストールするか、コントロールパネルの
「アプリケーションの追加と削除」からアンインストールしてください。


●使い方
登録すれば特に何もしなくてもHLT IDLEが有効になります。
ホストのタスクマネージャを見て、アイドル時にnp21/wのCPU使用率が
下がっていれば成功です。

他に、便利な可能性があるツールをいくつか付けています。
○NP2ICTRL.EXE
コマンドからHLT IDLEの有効無効を切り替えます。
オプションONで有効化、OFFで無効化、なしで現在状態表示です。

○NP2ITRAY.EXE
タスクトレイ常駐して右クリックでHLT IDLEの有効無効を切り替えます。
さらに、Wave再生中のHLTを禁止する付加機能もあります。
常駐が嫌いならHKLM\Software\\Microsoft\\Windows\\CurrentVersion\\Run
からNP2ITRAY.EXEを消してください。


●ソースコード
Neko Project 21/Wのソースに含まれます。
ライセンスはNeko Project II本体と同じ修正BSDライセンスに従います。


●更新履歴
2026/07/30 初版
2026/08/24 HLT IDLE有効時MP3再生などが不安定化するのをマシにした
2026/08/26 HLT Wave再生中のHLT禁止を追加、インストーラ作成

------------------------
Neko Project 21/W 開発者
SimK
