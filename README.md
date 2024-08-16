# DXRubynd

Ruby + Windows 用ゲームライブラリ [DXRuby](http://dxruby.osdn.jp/) を、インストール時にビルドする方式に変えたものです。そのためインストール時に DevKit が必要になりますが、代わりに Ruby のバージョンに依らずインストールが可能です。

なお オリジナル版にも言えることですが、最近の環境では [d3dx9_40.dll のインストールが必要になります。](https://github.com/mirichi/dxruby/issues/3)

## 使い方

使う人は

    gem install dxrubynd

でどうぞ。

```ruby
require 'dxrubynd'

Window.loop do
  # ここにゲームの処理を書く
end
```

1 行目は `require 'dxruby'` でも動くっぽいのですが、オリジナルの DXRuby もインストールされている環境では正常に動かない可能性があるので、`'dxrubynd'` の方を読み込ませたほうが確実です。

リファレンスマニュアルは
http://mirichi.github.io/dxruby-doc/index.html
にあります。

## 既知の問題点

<s>現状、64 bit 環境では正常に動作しません。オリジナル版でも [64 bit 版では Sound.new でエラーになる](https://github.com/mirichi/dxruby/issues/4)ようですが、こちらに至っては `require` しただけで `failed create window - CreateWindow (DXRuby::DXRubyError)` となってしまいます。解決方法をご存じの方はいらっしゃらないでしょうか…</s> → [DXRubyがRuby3.2.4(win64)環境で動かなかった件](https://sunnyday.hatenablog.jp/entry/2024/08/13/121029) の方法で解決しました。
