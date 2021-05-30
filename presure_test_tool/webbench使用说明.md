- webbench基本原理

  Webbench 首先 fork 出多个子进程，每个子进程都循环做web访问测试。子进程把访问的结果通过pipe 告诉父进程，父进程做最终的统计结果。

- webbench-1.5文件按夹下存放该工具的源码，在该文件夹下用**make**命令 进行编译，会生成可执行文件webbench。
- 使用方法：
  - webbench -c 1000 -t 30 http://10.24.88.50:10000/index.html 
  - 参数： 
    - -c 表示客户端数 
    - -t 表示时间

