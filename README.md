## STM32F407实现OX游戏（井字棋） + RT_Thread操作系统

Project 文件放置在 `bsp/stm32/stm32f407-atk-explorer/`

Keilv5

### 2025-05-06 版本一
能够完成放置棋子，判断三连胜利条件。
预计利用互斥锁保证“按键不干扰”，采用遍历的方式查询按键状态。
有问题，无法实现“按键不干扰”。
目前ox_game是单线程，而互斥量是多线程同步所使用的概念。
目前是循环打印图纸，非常不好。只能实现最初的功能。
考虑将按键和LCD屏幕打印分离开来，按键应该是中断服务例程，并且调整黄色选取框。
anyway

### 2025-05-06 版本二
实现按键线程和LCD屏幕打印线程分离，目前按键响应有些问题，考虑是硬件出错。
经过检查，确认是按键出现问题，回弹不及时。
考虑尝试利用其他方式来移动棋子 比如串口通信。

### 2025-05-07 版本2.1
实现串口通信方式移动棋子。
修改LCD界面打印函数，增加wasd移动说明等。
后续考虑增加获胜次数，玩家取名等交互式功能。

### 2025-05-09 版本2.2
修改串口通信的接收回调函数。
实现玩家取名功能，增加游戏重置模式（包括取名保留和重新取名）。
利用缓冲区来接收串口通信的内容，保证系统线程调度不受干扰。
