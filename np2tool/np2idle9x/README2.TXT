NP2IDLE 1.3.4
==============

Windows 9x 上で、Neko Project 21/W のアイドル時ホストCPU負荷を
HLTで低減するためのVxDと制御プログラムです。

構成
----
NP2IDLE.VXD       アイドル時HLT処理を行うVxD
NP2ITRAY.EXE      タスクトレイ常駐制御プログラム
NP2ICTRL.EXE      コマンドライン制御プログラム
SETUP.EXE         インストーラ/アンインストーラ

HLT処理
-------
VMMのCall_When_Idleから直接HLTせず、Call_Priority_VM_EventでSystem VMへ
処理を遅延させます。遅延処理の直前にVxDの有効状態とEFLAGS.IFを確認し、
HLT可能な場合だけHLTを実行します。

オーディオ再生時のHLT禁止
-------------------------
NP2ITRAY.EXEはWinMM Mixer APIを500ms周期で確認します。
MIXERLINE_COMPONENTTYPE_SRC_WAVEOUTのラインがACTIVEの場合、VxDへ1500msの
HLT禁止リースを送ります。再生が継続している間は500msごとにリースが更新されます。
更新が止まると最大1500msで自動的にHLT可能状態へ戻ります。

この機能はタスクトレイメニューの「オーディオ再生中はHLTしない」でON/OFFできます。
既定はONです。設定は次の場所へREG_DWORDとして保存します。

    HKEY_CURRENT_USER\Software\NP2IDLE
        AudioHltInhibit = 0 または 1

OFFへ切り替えた場合、現在のHLT禁止リースも直ちに解除します。

タスクトレイ操作
----------------
右クリックメニューから、VxD全体のHLT機能、オーディオ再生時のHLT禁止、
状態表示、このプログラムについて、終了を選択できます。

状態表示ではVxD状態、オーディオHLT禁止設定、WaveOut ACTIVE状態、Mixer数、
Active WaveOutライン数、VxD側のHLT禁止リース状態を確認できます。

CLI
---
    NP2ICTRL ON
    NP2ICTRL OFF
    NP2ICTRL STATUS

引数なしの場合もSTATUSです。

ビルド
------
VxD: Microsoft Windows 98 DDK + MASM 6.x
Win32アプリ: Visual C++ 6.0

VxDはVMM.INCより前に次を定義してWindows 95互換としています。

    WIN40COMPAT EQU 1

ビルドは次の1ファイルだけを使用します。

    build.bat

build.batは以前の出力を削除してから、VxD、CLI、トレイ常駐プログラム、
セットアッププログラムを一括でビルドします。

インストール
------------
SETUP.EXEを実行してください。通常のインストール先はシステムドライブ直下の
np2idle9ディレクトリです。たとえばWindowsがC:ドライブにある場合は次の場所です。

    C:\np2idle9

このディレクトリが存在しない場合は自動作成します。既に存在していても空なら確認せず
使用します。空でない場合だけ、既存ファイルを上書きする可能性があることを確認します。
VxDのロード用コピーはWindowsのSYSTEMディレクトリへ配置し、静的VxDとして登録します。
インストール後はWindowsの再起動が必要です。

アンインストール
----------------
コントロールパネルの「アプリケーションの追加と削除」、または

    SETUP.EXE /uninstall

を使用します。現在ユーザーのAudioHltInhibit設定も削除します。
