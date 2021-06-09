# Malloc visualizer

# data spec

```
# each row should be one of the following lines:
# (Numbers should be encoded in decimal.)
# allocate
a <begin_addr> <byte_size>
# free
f <begin_addr> <byte_size>
# map
m <begin_addr> <byte_size>
# unmap
u <begin_addr> <byte_size>
```
