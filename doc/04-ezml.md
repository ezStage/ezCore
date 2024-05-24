## 一种简单标记语言文档格式 ezML

version 1.0 （2024-5）

ezML是一种易于人编写和阅读，易于程序解析和使用的文本数据文档格式，有以下特点：

- 采用 UTF-8 编码格式；

- 采用键/值对赋值的方式(key=value)，同时也支持只有 key 或只有 value；

- value支持字符串，整数，浮点数，字典(dict)四种类型，字典就是多个键/值对的序列；

- value可以附加一个FourCC标记，FourCC标记只包括最多4个字母，数字和下划线；

一个ezML文档格式的例子

```ezml
weibo="https://weibo.com/leiqikui"
github = "https://github.com/leiqikui"
qq={
  index=1
  number ="376601179"
  email= "376601179@qq.com"
}
sites={ "Google.com" "Taobao.com" "Waibo.wang" }
```

### 字符集

ezML 采用UTF-8编码字符集。特殊字符如下表，都是英文字符：

|        |        |     |        |        |     |
| ------ | ------ | --- | ------ | ------ | --- |
| '  单引号 | "  双引号 | .   | {    } | [    ] | =   |
| \      | ^      | @   | #      | ,  逗号  |     |

### 词法元素

有以下几种词法元素：

#### 1. label 标识符

以英文字母或下划线开始，后面连续跟英文字母、数字、下划线的单词

#### 2. int 整数

整数有以下几种形式：

- 十进制整数：以 + 或 - 开始或没有正负号，后面是连续的0-9数字。

- 十六进制整数：以 + 或 - 开始或没有正负号，后面是0x或0X，再后面是连续的0-9,A-F或a-F

#### 3. float 浮点数

因为markdown不能很好的书写文法，所以用几个示例来说明支持的浮点数形式：

- +123.456

- -123.456e+7

- 123e8

- 0.456E-9

#### 4. 单引号字符串

两个单引号中的字符序列，字符串中可以有转义。

#### 5. 双引号字符串

两个双引号中的字符序列，字符串中可以有转义。

两种类型的字符串中除了转义符，其它所有符号都保持原样。两种类型的字符串中支持相同的转义符，见下表：

| 转义符  | 说明     | 值    |
| ---- | ------ | ---- |
| \0   | 0      | 0x0  |
| \a   | 报警符    | 0x07 |
| \b   | 退格符    | 0x08 |
| \f   | 换页符    | 0x0c |
| \n   | 换行符    | 0x0a |
| \r   | 回车符    | 0x0d |
| \t   | 水平制表符  | 0x09 |
| \v   | 垂直制表符  | 0x0b |
| \'   | 单引号    | ’    |
| \"   | 双引号    | ”    |
| \\\  | 反斜杠    | \    |
| \xAb | 十六进制字节 | 0xab |

#### 6. fourCC

fourCC 全称 Four-Character Codes，代表四字符代码 (four character code)，它可以用一个无符号32位整数来表示。@符号后面跟最多四个英文字母、数字和下划线的字符序列组成fourCC，@ 和 fourCC 之间不能有空白符号。比如 `@1234` 表示 0x34333231。

每个 Value 后面可以跟一个fourCC，可以看作为 Value 的单位，比如 `12@pt`。

#### 7. note 注释

不在字符串中的#号是注释开始，直到这一行结束。在字符串中的#号是普通符号。

### 语法结构

#### 赋值语句

赋值语句有以下几种形式：

- key=value

- key

- value

- `key=value@fourCC`

- `key@fourCC`

- `value@fourcc`

#### key

key有如下几种形式：

- label 标识符

- 单引号字符串

- ^  (表示没有key。即 `^ = value` 等价于 `value`)

- 上面几种形式用 . 号分隔的key序列

#### value

value 有以下几种形式：

- int 整数

- float 浮点数

- 双引号字符串

- {  赋值语句序列  }

key 的开始符号为 字母、下划线、单引号、^ 。value 的开始符号为 +、-、数字、双引号、{ 。key和value的开始符号不冲突，该文法是LL(1)文法，没有歧义，不会回溯。并且在两个赋值语句中间不需要分隔符，当然也支持英文逗号分隔。

注意多个连续的逗号也只算一个隔符号。

如果要创建一个既没有key，又没有value的节点，可以用 `^,`

#### [key]

key总是相对于当前 dict 定位，但是 [key] 是相对于根 dict 绝对定位。

### 例子

```ezml
class={
  group={
    name="zhang"
    age=10
  }
}

#等价于:
class.group={
    name="zhang"
    age=10
}

#等价于:
class.group.name="zhang"
class.group.age=10

#等价于:
[class.group]
name="zhang"
age=10
```

```ezml
class={
  group={
    {
      name="zhang"
      age=10
    }
  }
}

#等价于:
class.group={
  {
    name="zhang"
    age=10
  }
}

#等价于:
class.group.^={
  name="zhang"
  age=10
}

#等价于:
[class.group]
{
  name="zhang"
  age=10
}

#等价于:
[class.group.^]
name="zhang"
age=10
```

```
class={
  group1={
    name="zhang"
    age=10
  }
  group2={
    name="wang"
    age=10
  }
}

#等价于:
class.group1={
    name="zhang"
    age=10
}
class.group2={
    name="wang"
    age=10
}

#等价于:
[class]
group1={
  name="zhang"
  age=10
}
group2={
  name="wang"
  age=10
}

#等价于:
[class.group1]
name="zhang"
age=10

[class.group2]
name="wang"
age=10
```

注意不能在 {  } 中嵌套 [  ] 。
