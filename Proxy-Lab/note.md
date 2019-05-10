driver.sh是测评脚本

function download_proxy {
	cd $1
	curl --max-time ${TIMEOUT} --silent --proxy $4 --output $2 $3
}

function download_noproxy {
	cd $1
	curl --max-time ${TIMEOUT} --silent --output $2 $3 
}

PART I通过
download_proxy $PROXY_DIR ${file} "http://localhost:${tiny_port}/${file}" "http://localhost:${proxy_port}"
download_noproxy $NOPROXY_DIR ${file} "http://localhost:${tiny_port}/${file}"
比对输出文件。

PART I:
	实验中，观察到firefox设置代理后, 
	http请求头不变, 方法为GET， URL为输入在浏览器中的http开头的地址，版本为http/1.1, host为url中第一个/前的字符串;
	而https的请求头发生了改变，方法变为CONNECT，url为xxxx:443，版本为http/1.1，host同url，而https在未设置代理的时候，是加密传输的

PART II
./tiny ${tiny_port} &> /dev/null &
./proxy ${proxy_port} &> /dev/null &
./nop-server.py ${nop_port} &> /dev/null &

download_proxy $PROXY_DIR "nop-file.txt" "http://localhost:${nop_port}/nop-file.txt" "http://localhost:${proxy_port}" &

download_noproxy $NOPROXY_DIR ${FETCH_FILE} "http://localhost:${tiny_port}/${FETCH_FILE}"
download_proxy $PROXY_DIR ${FETCH_FILE} "http://localhost:${tiny_port}/${FETCH_FILE}" "http://localhost:${proxy_port}"
diff -q ${PROXY_DIR}/${FETCH_FILE} ${NOPROXY_DIR}/${FETCH_FILE}

测试程序
	tiny_port服务端口, proxy_port代理端口, nop_port被接上后永远不断开
	通过代理端口访问nop_port，代理程序内的线程1始终不会结束;
	再正常访问, 得到文件1;
	最后再通过代理端口访问, 代理程序内的线程2得到文件2;
	比对文件1和文件2

PART II:
	多线程编程，每次监听到客户端连接后，调用Pthread_create来创建线程，通过malloc一个字节大小的内存存放connfd，再由创建出来的线程执行free操作。
	也可以将connfd参数通过传值传递给线程thread函数。



PART III
./tiny ${tiny_port} &> /dev/null &
./proxy ${proxy_port} &> /dev/null &
for file in ${CACHE_LIST}
do
    echo "Fetching ./tiny/${file} into ${PROXY_DIR} using the proxy"
    download_proxy $PROXY_DIR ${file} "http://localhost:${tiny_port}/${file}" "http://localhost:${proxy_port}"
done

download_proxy $NOPROXY_DIR ${FETCH_FILE} "http://localhost:${tiny_port}/${FETCH_FILE}" "http://localhost:${proxy_port}"

diff -q ./tiny/${FETCH_FILE} ${NOPROXY_DIR}/${FETCH_FILE}  &> /dev/null

开启tiny_port服务端口, proxy_port代理端口
对CACHE_LIST内的每个文件, 通过proxy_port代理端口访问一次服务端口, 缓存
之后关闭tiny_port服务端口,
再通过proxy_port代理端口访问FETCH_FILE文件, 服务端关闭只能访问缓存, 比对得到的文件与源文件。

PART III:
	使用读者写者(读者优先)算法. 
	每次接收到一个请求, 顺序访问缓冲区, 挨个加读锁, 如果cache命中, 则将缓冲区的内容输出到套接字,直到输出完毕, 再解除读锁.
	如果cache miss, 则将内容输出到套接字, 同时在本地保存内容的副本, 并在套接字关闭后, 保存于cache中. 调用put接口, 挨个读取, 如果缓冲区不满, 则某无效缓存object即为应当替换的object, 否则, 按LRU算法找到应当被替换的object. 加写锁, 保存副本.


