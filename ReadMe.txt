AAC ADTS Editor 0.1 alpha 3 
http://2sen.dip.jp:81/cgi-bin/friioup/source/up0164.zip 
に、以下の改変を行ったものです。
基本的な使い方は、オリジナルのreadme.txtを参照してください。

① -d オプションが正しく反映されないのを修正

② -x オプション追加
-x オプションを指定すると、そのファイル名から、-d/-o オプションを自動生成する。
例えば、
aacedit.exe "hoge 01 (delay -29ms).aac" -x
は、
aacedit.exe "hoge 01 (delay -29ms).aac" -o "hoge 01 (delay 0ms).aac" -d -29
と同じ動作をする。(マイナスのディレイのみ有効)

この改変版にトラブルがあっても、 ◆SP2R1101W氏には苦情を言わないように！

