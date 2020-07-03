# Hello World Example

Starts a FreeRTOS task to print "Hello World"

See the README.md file in the upper level 'examples' directory for more information about examples.

#1. First ,you will get ssd1306 port library from tony into esp-idf componets lib
```
git clone https://github.com/materone/ssd1306.git
```
#2. Then checkout the esp32 build version
```
git checkout esp32-v1
```
#3. change the code ssd1306news to v4 ,adptor the esp-idf v4+
```
git clone https://github.com/materone/ssd1306news.git
git checkout v4
mkdir ssd1306news/components
cp -a ssd1306 ssd1306news/components
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
#Next
   ```flow
    st=>start: Start|past:>http://www.baidu.com
    e=>end:  Ende|future:>http://www.baidu.com
    op1=>operation:  My Operation
    op2=>operation:  Stuff|current
    sub1=>subroutine:  My Subroutine|invalid
    cond=>condition:  Yes or No|approved:>http://www.google.com
    c2=>condition:  Good idea|rejected
    io=>inputoutput:  catch something...|future
    st->op1(right)->cond
    cond(yes, right)->c2
    cond(no)->sub1(left)->op1
    c2(yes)->io->e
    c2(no)->op2->e
```
```mermaid
graph TD;
  A-->B;
  A-->C;
  B-->D;
  C-->D;
```
