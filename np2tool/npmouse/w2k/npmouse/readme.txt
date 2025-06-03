■■■ Neko Project II シームレスマウスドライバ for Win2000 ■■■

Windows 2000においてNeko Project II上でのシームレスマウス操作を実現します。
【注意！！！】
手順を間違えるとマウスが使えなくなったりブルースクリーンで起動不能になったりします。
マウスが使えない分はキーボード操作で回復できますが、ブルースクリーンの場合は別OSで起動して、
WINNT\System32\driversにあるnpmouse.sysを削除する必要があり、復旧が大変です。
先にイメージごとバックアップを取っておくのが簡単でおすすめです。

●動作環境
【ゲスト】
Windows 2000
【ホスト】
Neko Project 21/W ver0.86 rev94β11以降
Device->Mouse->Non-capture Controlが有効な環境


●インストール
サービスのインストールとマウスドライバへのフィルタ登録が必要です。
サービスはnpmouse.infを右クリック→インストール
でインストールできます。
アップデートも同じ操作でできますが、反映にはシステムの再起動が必要です。
マウスドライバへのフィルタ登録は、regedt32（regeditではない）を起動し、
HKEY_LOCAL_MACHINE\SYSTEM\ControlSet001\Control\Class\{4D36E96F-E325-11CE-BFC1-08002BE10318}
へ移動します。その中にあるUpperFiltersがmouclassであることを確認します。
そうでない環境は危ないので中断してください。
問題なさそうなら、UpperFiltersの項目をダブルクリックし、以下の2行の内容にします
mouclass
npmouse
順番も大事ですので、逆にしないでください。
これで再起動すればシームレスマウス操作が有効になります。

シームレス操作は
①マウスキャプチャ状態ではない
②Device→Mouse→Non-capture ControlがON
の場合に有効です。


●アンインストール
必ずマウスドライバへのフィルタ登録解除を先に行ってください。
順番を逆にするとマウスドライバがエラーになり、マウス操作ができなくなります。
（キーボード操作だけで頑張れば復旧は出来ます）
regedt32（regeditではない）を起動し、
HKEY_LOCAL_MACHINE\SYSTEM\ControlSet001\Control\Class\{4D36E96F-E325-11CE-BFC1-08002BE10318}
へ移動します。その中にあるUpperFiltersをmouclassの1行だけに戻します。
その後、一旦再起動してシームレス操作が無効化されていることを確認してください。
シームレス操作が無効化されていることを確認したら、レジストリの
HKEY_LOCAL_MACHINE\SYSTEM\CurrentControlSet\Services\npmouse
を削除し、WINNT\System32\driversにあるnpmouse.sysを削除すればアンインストール完了です。


●使い方
シームレス操作は以下の条件を満たす場合に自動的に有効になります。
①マウスキャプチャ状態ではない
②Device→Mouse→Non-capture ControlがON


●ソースコード
srcフォルダにソースコードがあります。
いちおう猫本体と同じ修正BSDライセンスとしますが、実質的に自由に使っていただいて大丈夫です。


●ぼやき
本当はWinNT系全てをカバーするつもりでしたが、想定以上にOS間の差が大きすぎて断念しました。
やること自体はOSへ送るマウスデータの座標を絶対座標にし、絶対座標フラグを立てるというただそれだけなんですが、
一般に出回っている情報がPS/2マウス前提でPC-98では同じ手が使えないと気付くのに2日かかったりしました。
WinNT4.0以前で正常に動かす方法は未だに見つかっていません。
Win9xや3.1でも価値があるはずですので、気が向いた人はぜひドライバか何かを書いてみてください。
なお、np21/wからマウス絶対座標を取得する情報はサイトの方に書きました。
https://simk98.github.io/np21w/docs/npcngcfg.html


------------------------
Neko Project 21/W 開発者
SimK
