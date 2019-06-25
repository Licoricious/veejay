Veejay/Android
================
Veejay can be controled throught an Android App : VJ remote control App

The veejay server **must be compiled** with the [libqrencode](https://fukuchi.org/works/qrencode/) dependency.

When veejay is compiled with QR code support, you can give it the
'qrcode-connection-info' commandline switch:
```
$ veejay ... -qrcode-connection-info
```
In Veejay, press `CTRL-C` to display the QR code into the video output

If you have the android VJ remote control App, you can connect to veejay by
entering the veejay's ip address or by scanning veejay's QR code.

Technical:
----------
Veejay's server address is encoded as string `HOST:PORTNUMBER`, for example
`192.168.10.10:3490` or `veejay-pc.dark.net:3490`

The QR image is regenerated at each startup and placed in `$HOME/.veejay` as `QR-3490.png`

See also [How to Compile](./HOWTO.compile.md) for precise compilation information.
