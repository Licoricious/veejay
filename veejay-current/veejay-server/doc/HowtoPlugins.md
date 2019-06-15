Plugins
========

By default, veejay looks in the following commmon locations to find plugins:
```
- /usr/local/lib
- /usr/lib/
- /usr/local/lib64
- /usr/lib64
```

Alternatively, you can create a file to tell veejay where to find plugins.
```
 $ mkdir ~/.veejay
 $ vi ~/.veejay/plugins
```

Apart from the differences due to your OS, the contents of the file can look like:
```
/usr/local/lib/freeframe
/usr/local/lib/frei0r-1
```

Veejay will pick up the plugins the next time you start it.
