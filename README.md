# Hello World Example

Starts a FreeRTOS task to print "Hello World"

See the README.md file in the upper level 'examples' directory for more information about examples.

1. First ,you will get ssd1306 port library from tony into esp-idf componets lib
```
git clone https://github.com/materone/ssd1306.git
```
2. Then checkout the esp32 build version
```
git checkout esp32-v1
```
3. change the code ssd1306news to v4 ,adptor the esp-idf v4+
```
git checkout v4
```

```flow
st=>start: 开始
op=>operation: My Operation
cond=>condition: Yes or No?
e=>end
st->op->cond
cond(yes)->e
cond(no)->op
```
