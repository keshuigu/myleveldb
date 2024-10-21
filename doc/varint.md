[Base 128 Varints](https://protobuf.dev/programming-guides/encoding/#varints)

1. 每个字节第一位为MSB，标识该字节是否为最后一个字节，0代表为最后一个字节
2. 小端序转为大端序
3. 连接并得到数字

1. 对于负数，使用zigzag映射到正整数