// v7.8 新增音乐页：导航左滑天气，右滑音乐；音乐左滑返回，上下滑也可查看天气；上一首/播放暂停/下一首，中文歌名和歌手。
// MotoNav 圆屏固件：蓝牙接收导航指令，显示箭头、距离和各种行车信息
// 板子：微雪 ESP32-S3-Touch-AMOLED-1.75（466x466）
// 配套手机 App：v2.0 及以上
//
// 版本记录
//   v1.0  蓝牙接收指令，显示箭头和距离
//   v1.1  改用内存画布绘图，修复箭头不显示
//   v1.2  修复串口打印卡住导致的约 4 秒延迟
//   v1.3  断线保护：蓝牙断开时不再停留在旧箭头；导航中超过 6 秒收不到数据，
//         屏幕边缘亮红圈并提示 NO DATA；支持心跳指令 "P"
//   v1.3.1 修复编译错误（drawWaiting 的参数类型）
//   v1.4  所有箭头按外框居中显示；NO DATA 提示移到距离数字下方
//   v1.5  三阶段转向提示（远：直行线+灰色预告；中：大箭头+黄色进度环；近：橙色）
//   v1.6  拐弯处改为圆角；远处预告竖条居中；进度环连续平滑、动画追赶
//   v1.7  新增：顶部车速、电子眼限速牌（超速变红）、车道提示、
//         中文路名和底部“剩余里程·到达时间·红绿灯数”（由手机渲染成图片发来）、
//         亮度调节；蓝牙改为二进制消息队列，支持最长 512 字节的数据包
//   v1.7.1 整体排版上移，路名和底部信息离外圈更远；文字图片太宽时自动缩小，
//         保证不碰到圆环；有车道时距离数字稍小一号（配套 App v1.7.1）
//   v1.7.2 车道里的左转、右转小图标改为直角样式（竖着上去，再横着拐出去）
//   v1.7.3 底部“剩余里程·到达时间·红绿灯数”常驻显示（出现车道时也不再隐藏）
//   v1.8  转弯箭头角度跟随路口实际角度（手机算好发来），锐角弯显示锐角箭头
//         （配套 App v1.8；旧 App 不发角度时照旧用标准角度）
//   v1.8.1 大图标和远处预告改用“抗锯齿矢量绘制”：转弯处是真正连续的圆弧，边缘平滑无锯齿、
//         无拼接痕迹；小角度转弯的弯也更圆润。图标画好后缓存，画面刷新时直接复制，不增加耗时
//   v1.8.2 图标自动适应可用区域：不管箭头是什么角度（缓弯的箭头特别高），都整体缩放并居中到
//         “车速下方 ~ 车道/距离上方”之间，不再挡住 km/h
//   v1.8.3 进度环、超速红圈、断线红圈、状态点、限速牌改为抗锯齿绘制，边缘平滑无毛边
//         （开机时预先算好圆环上每个像素的半径和角度，每帧只查表，速度很快）
//   v2.0  小地图（罪恶都市风格）：手机发来附近的道路/绿地/水面（地图块）和导航路线，
//         圆屏画成车头朝上、跟着转的小地图；左下角小圆牌显示转向和距离，上方显示车速。
//         离路口 150 米内自动切回大箭头画面。新增蓝牙通知通道，收齐地图后回确认给手机。
//         （配套 App v2.0）
//   v2.1  小地图一直显示，路口前不再跳回大箭头画面：离路口 150 米内左下角圆牌放大、
//         边框变橙色，车道图标也显示在圆牌里；100 米内箭头和进度环变橙色
//   v2.2  提高小地图帧率：双缓冲 + 双核并行（一个核画下一帧，另一个核同时把上一帧推到屏幕）；
//         取消 60 毫秒的帧间隔限制；道路只在拐弯处补圆角，少画很多圆。
//         每 2 秒把帧率、画图耗时、刷屏耗时发给手机，App 上可以看到（配套 App v2.0.1）
//   v2.3  减轻画面撕裂：屏幕数据线速度可在 App 里切换 40 / 60 / 80 MHz（保存在圆屏里，切换后自动重启）。
//         速度越快，每帧推到屏幕的时间越短，撕裂越轻；如果花屏就切回 40。（配套 App v2.1）
//   v2.4  小地图左下角圆牌：离路口 500 米以外显示直行箭头（和大箭头画面的远阶段一致），
//         500 米以内才显示转向箭头，不再几公里外就提示转弯（配套 App v2.2：测速提醒提前）
//   v2.5  支持天地图“图片地图”（大块数据类型 3）：手机把天地图底图归类成 街区/道路/绿地/水面 四色、
//         压缩后发来，圆屏每帧按车头方向旋转显示；线条地图（OpenStreetMap）照常支持，
//         两种都有时显示最新收到的那种（配套 App v2.3）
//   v2.6  提高帧率：
//         1. 快速刷屏：自己驱动屏幕数据线，一边整理下一段像素一边传上一段（原来是排队一段一段来），
//            可在 App 里关掉（花屏时用）；
//         2. 图片地图按 16×16 小块存放，旋转取像素时内存命中率高很多；
//         3. 整圈的圆环（黑边、底环、红边）改为按行填充，不再逐点查表；
//         4. 新位置到达时，和推算位置之间的偏差在 0.4 秒内平滑修正，地图不再“跳一下”；
//         5. 帧率统计里多报“画地图”“画边框信息”各用多少毫秒（配套 App v2.4）
//   v2.7  小地图信息区重新设计：去掉左下角突兀的小圆牌和车速胶囊，改成和黑色外圈连成一体的
//         上下两块“弧形面板”——上面显示车速/测速，下面显示转向箭头和距离；
//         离路口 150 米内下面板升高、分隔线变橙色，并显示车道
//   v2.8  按要求改回整屏地图：去掉上下面板，车速、转向箭头、距离直接浮在地图上，
//         带黑色描边保证看得清；车道放在一条半透明深色底上（地图仍然透得过来）
//   v2.9  更有 GTA 味：
//         1. 地图配色主题：罪恶都市（默认，暖沙色街区、青绿水面、粉紫路线、青色车速）/
//            GTA5（深色地面、浅色道路、紫色路线）/ 原版，可在 App 里切换（"#t,0/1/2"）；
//         2. 雷达边缘的“N”北向标记，跟着车头方向转；
//         3. 目的地标记：在雷达范围内显示在地图上，范围外贴在雷达边缘指出方向；
//         4. 雷达边缘一圈渐暗阴影，更像游戏里的圆形雷达（配套 App v2.5）
//   v3.0  1. 走过的路线变灰（或直接擦掉，App 里选），只有前方的路线是鲜艳的颜色；
//         2. 夜间配色：三套主题都有夜间版（地面压暗、道路变浅色线条、路线保持鲜艳），
//            手机按当地日出日落自动切换，也可以固定白天/夜间（配套 App v2.8）
//   v3.1  小地图底部加一行行程信息：到达时间 · 剩余里程 · 红绿灯数（离路口 150 米内让位给车道，自动隐藏）；
//         前方 300 米内有红绿灯时，左侧显示红绿灯图标和距离（配套 App v3.1）
//   v3.2  红绿灯直接画在小地图上它实际的位置（只画还没开过的），下一个灯在 300 米内时旁边标距离；
//         去掉 v3.1 左侧的红绿灯面板（配套 App v3.3）
//   v3.3  小地图的车道提示从底部移到车速下面，并放大一点（不再和底部的 N 标记、外圈挤在一起）
//   v3.4  车道图标抗锯齿：边缘平滑，左转/右转/掉头的拐角改成圆弧（大箭头界面的车道也一样）
//   v3.5  电子眼画在小地图上的实际位置：测速点是带限速数字的小限速牌，其他拍照点是相机图标；
//         开过以后自动消失（配套 App v3.4）
//   v3.6  首页加一只像素猫猫：连上手机时睁眼、摇尾巴、偶尔眨眼；没连上手机时闭眼睡觉，头上飘 z
//   v3.7  转弯更顺：小地图转向改成按时间平滑转动（最快每秒 70 度），缩放也按时间平滑，
//         相邻两帧变化更小更均匀，撕裂感更轻（这块板子没接屏幕刷新同步信号，撕裂无法完全消除）
//   v3.8  小地图上的电子眼按类型画不同图标（测速牌、监控、闯红灯、违章拍照、公交道、应急车道、
//         非机动车道、区间起点/终点、流动测速、ETC、礼让行人、违法变道、违停、禁止鸣笛）；
//         图标优先画在实际位置，和别的东西（另一个电子眼、红绿灯、车头、北向标记、车速、底部信息）
//         重叠时自动挪到旁边的空位，并用细线连回实际位置
//   v3.9  电子眼图标挪开时紧贴在实际位置旁边（不再拉长线，实际位置画个小白点）；车头箭头缩小到约 70%
//   v4.0  电子眼图标位置选好后锁住不动，转弯转稳后才重新选一次；停车/暂停后一直保持地图画面
//         （以前 5 秒收不到位置就切回大箭头画面）
//   v4.1  路上只画蓝色小摄像头（代替小白点）和红绿灯；类型图标（测速牌、违章拍照……）一律摆在
//         小摄像头旁边，优先放在道路两侧，不压路线，图标之间留空隙不重叠
//   v4.2  修复两个已锁住位置的类型图标叠在一起：锁住的图标和别的电子眼图标重叠时重新选位置
//   v4.3  同一处有好几个电子眼时，类型图标排成一列（和道路平行）整列放在道路一侧，间距固定不会重叠；
//         整列一起锁住（v4.2 的“重叠就重选”去掉了，它会让图标来回跳）；两侧都放不下时放到远一列，用细线连回
//   v4.4  导入高德分享路线导航时，原路线画成一条橙色细线（在导航路线下面，走错路时才露出来）；
//         新增大块数据类型 4 = 原路线，"#o" = 不显示原路线
//   v4.5  蓝牙允许 2M 模式和最长空中数据包，手机请求时直接用（大块数据传得更快）
//   v4.6  首页的猫猫换成原创像素女骑手和小摩托：连上手机时围巾马尾飘动、眨眼、挥手；没连上时车灯熄灭、打瞌睡
//   v4.7  修复类型图标被推到远处、拉一根长线：小摄像头和图标之间改按圆形判断重叠（以前按方框，斜放时误判）
//   v4.8  巡航模式（"#z,1/0"）：没有目的地时也显示小地图、车速、电子眼和限速；
//         道路安全提示图标（急弯、学校、事故多发、注意行人、陡坡、村庄、窄桥、落石、隧道、注意，编号 16~25），
//         路上画小黄点、图标摆在旁边；电子眼最多 10 个；修复文字消息超过 63 字节被截断（电子眼多了会丢）
//   v4.9  清爽模式（"#w,1/0"）：小地图照常显示，去掉红绿灯、电子眼、安全提示、测速牌、超速红圈和底部行程信息，
//         只留地图、路线、车速、转弯箭头/距离、车道和进度环
//   v5.0  路况颜色（"#u"：缓行黄、拥堵红、严重拥堵深红）；走错路时显示 REROUTING（"#y,1/0"）；
//         发现更快路线时显示 FASTER ROUTE -X min（"#h,分钟"）；GPS 信号弱时左上角亮 GPS 图标（"#x,1/0"）
//   v5.1  来电显示（"#i,1,号码" / "#i,0"，名字是图片栏 2 的文字图片）：来电时整屏显示是谁，接听或挂断后回到原画面
//   v5.2  触屏接电话：来电画面上按住屏幕任意位置 0.8 秒（绿圈外一圈白色进度走满）就接听，
//         通过通知通道告诉手机 "T,A"；按下时也报一次坐标 "T,P,x,y"（以后做左右按钮用）。
//         触摸芯片 CST9217（SensorLib 库），只在来电时读取
//   v5.3  天气页：在圆屏上左右滑（上下滑也行）切换到天气，再滑一下回来。手机用高德天气查当地实况和
//         4 天预报发来（"#W,图标,温度,更新时间,天数,{星期,图标,最高,最低}…"；地名·天气、风·湿度是
//         图片栏 3、4 的文字图片）。导航中看天气 15 秒后、或快到路口（200 米内）自动回到导航画面。
//         触摸改为一直监听（触摸芯片有动静才读），切到天气页时向手机要一次数据 "T,W"
//   v5.4  换页滑动动画：跟着手指方向，旧画面滑出、新画面滑入（约 0.3 秒，先快后慢）；
//         自动回导航时按原路滑回去。触摸方向和画面相反的话，改 TOUCH_FLIP_X / TOUCH_FLIP_Y
//   v5.5  换页改成“跟手”：手指按住拖动时新页面跟着手指一点点滑进来，松手后按速度顺势滑完；
//         拖得不够远、也没甩出去就弹回原页面。新页面盖在上面，旧页面慢慢往后退（视差）、
//         渐渐变暗，交界处有一道阴影；动画用两块画布轮流推屏，更顺滑
//   v5.5.1 触摸左右方向反了：TOUCH_FLIP_X 改为 1
//   v5.6  天气页下面一排改成接下来 5 个小时：几点、图标、温度，会下雨时显示降雨概率（蓝色）。
//         "#W,图标,温度,更新时间,H,个数,{几点,图标,温度,降雨%}…"；手机查不到逐小时时发 D（按天，和以前一样）
//   v5.7  小地图提速：
//         1) 天地图底图（每帧最费时的逐像素计算）改成两个核心一起画：主核心画偶数行，另一个核心画奇数行。
//            另一个核心平时只负责推屏，推屏大部分时间在等数据线传完，正好空着
//         2) 雷达边缘渐暗：开机时把每个像素的暗度算好存表，每帧只查表（原来每个像素都要开平方、做 3 次除法）
//         3) 帧率统计里多报一项“底图用时”，方便看瓶颈在哪
//   v5.8  实测 8.9 帧：底图 42 毫秒、边框和信息 53 毫秒。
//         1) 帮忙画底图的任务优先级提到推屏任务之上（原来推屏一直占着那个核心，帮忙的任务排不上，
//            主核心画完自己那一半只能干等）
//         2) 描边文字（车速、距离）原来要把字重复画 24 遍，改成 16 遍，效果一样
//         3) 每帧各步骤分别计时，每 2 秒发一份 "Q,…" 给手机（单位 0.1 毫秒），找出真正慢的地方
//   v5.9  按实测分步用时继续提速（13 帧时：外圈 22.7 毫秒、边缘渐暗 10.3 毫秒）：
//         1) 外圈（黑边 + 进度环底 + 进度条 + 超速红圈）原来分 4 次画，进度条还要把一圈圈像素在屏幕上
//            来回跳着访问（很费外挂内存）。改成一次从上到下逐行算出最终颜色，两个核心各画一半的行
//         2) 边缘渐暗也改成逐行、两个核心一起画，暗度按“半径平方”查一张放在芯片内部内存里的小表
//         3) 混色改用乘法加移位，不再做除法
//   v6.0  v5.9 的外圈反而更慢（48 毫秒）：这块芯片的浮点开平方、除法是软件算的，很慢，而外圈每个像素都开了平方。
//         1) 外圈改成纯整数：半径用“半径平方 → 半径”的小查表（放芯片内部内存），进度条的角度判断改成
//            两次乘法（叉乘）；圆外角落直接整段清零
//         2) 两个核心分活改成“上半屏 / 下半屏”，不再隔行交错（交错时两个核心抢同一块缓存，互相拖慢）
//         3) 转弯撕裂：屏幕没有接刷新同步信号，帧率低时相邻两帧角度差太大就会看到明显的错位。
//            现在每帧最多转 3.5°（帧率高时不影响，帧率低时画面转得稍慢一点，但不再撕得厉害）
//   v6.1  新增 8 种地图风格（"#t,3"~"#t,10"）：
//           3 赛博霓虹  4 复古纸地图（夜间配色时自动变成霓虹）  5 极简黑白  6 蓝图（跟着地图走的网格）
//           7 掌机像素（底图按 3×3 大像素画，还更快）  8 雷达（转动的扫描线 + 距离圈）
//           9 寻宝罗盘（六边形网格、跟着车头转的罗盘刻度、目的地菱形 + 脉冲圈）
//           10 卫星图（手机把天地图卫星照片压成 16 色发来，地图数据里自带调色板）
//         另加“弯道急缓上色”开关（v6.2 起是 "#Q,1/0"）：前方路线按弯急程度涂成绿 / 黄 / 红，任何风格都能叠加
//   v6.1.1 赛博霓虹的道路青色调暗一点，让品红路线更跳
//   v6.2  圆屏方向：整个画面可以转 0° / 90° / 180° / 270°（"#R,0~3"，存在板子里，开机就按这个方向），
//         装车时线从哪边出来都行。转向在推屏时顺手完成（不多花画图时间）；触摸方向跟着一起转。
//         修复：v6.1 的弯道上色用了 "#q"，和“屏幕数据线速度”撞车，一直开不了 → 改成 "#Q,1/0"
//   v6.4  圆屏方向支持任意角度（"#D角度"，0~359，没有逗号，旧固件会忽略它）：App 上转圆盘，圆屏实时跟着转。
//         90° 的整数倍照旧走原来的快速通道；其他角度推屏时逐像素按角度取样（只算圆里面的像素），
//         帧率会略低一点。手指滑动方向也按角度换算。角度停下 1.5 秒后才存进板子（拖动时不反复写闪存）。
//         （配套安卓 App v6.3；Mac 上 iOS 那边做的 v6.3 固件用的指令可能不同，以这一版为准再对齐）
//   v6.5  蓝牙配对验证：陌生手机连上也发不了任何指令。
//         - 第一次连接时圆屏显示 6 位配对码，在手机弹出的配对框里输入；配好后手机被记住（最多 3 台），以后自动连、全程加密
//         - 只有“开机 2 分钟内”或“还没有配对过任何手机”时才显示配对码；其他时候有陌生手机来配对，
//           圆屏显示 LOCKED，配对必然失败（要加新手机：重启圆屏，2 分钟内配对）
//         - 连上 60 秒还没通过验证的连接自动断开，不会占着圆屏让你自己的手机连不上
//         - 通过验证后通知手机 "U,1"（App 收到后把所有状态重发一遍）；"#UNPAIR" 清除记住的所有手机
//   v6.6  修："#UNPAIR" 把正在用的手机也忘了，可手机那边还记着配对 → 两边对不上，再也连不上。
//         改成只忘掉“别的”手机，正在连着的这台保留（丢手机 / 换手机时用新手机点一下就行）
//   v6.7  小地图跟得更紧（减少“慢半拍”）：
//         1. 转弯预判：用前后两次定位算出正在以多快的速度转弯，两次定位之间地图按这个速度继续转，
//            位置也沿着弯道推算（原来只会沿直线推、朝向要等下一次定位才变）
//         2. 提前量：画面显示“再过一点点时间”的位置和朝向，抵消 GPS 和传输的延迟（"#L毫秒"，0~800，默认 300；
//            App 里可选 关 / 0.3 秒 / 0.6 秒）
//         3. 地图朝向追得更快（时间常数 0.35 → 0.22 秒）
//         4. 不认识的 "#..." 指令一律忽略（以前会被误当成导航方向）
//   v6.8  电量显示（接了锂电池时）：板子上的 AXP2101 电源芯片读电量、是否在充电、是否插着 USB。
//         圆屏顶部（蓝牙小圆点右边 / 小地图车速上方）画一个小电池和百分比：>50% 绿、20~50% 黄、<20% 红，
//         充电时青色带小闪电；低于 10% 没在充电时闪烁。没接电池就不画。
//         电量变化或每分钟发给手机一次："V,百分比,是否充电,是否插USB,电压mV"
//   v6.9  来电按住接听从 0.8 秒缩短到 0.2 秒（骑车时手离开车把的时间越短越安全）；
//         提示文字改成 "TAP & HOLD"，白点进度圈照旧（0.2 秒走满）
//   v7.0  蓝牙名带上自己硬件地址的最后 4 位（例 "MotoNav-3A7F"），几块圆屏在一起不会混；
//         这 4 位和手机蓝牙设置里看到的地址末尾一样。首页和配对页都显示这个名字
//   v7.1  到达特效：到达目的地时约 7 秒庆祝动画（放射光芒、冲击波、烟花、飘动的终点旗、ARRIVED 霓虹字、
//         行程统计逐字打出）；到达途经点 3 秒短版。点一下屏幕提前结束。
//         新指令（不带逗号，旧固件会忽略）："#O米/分钟/最高车速" 行程统计（在 "A,0" 前发），"#J" 到达途经点
//   v7.2  修：首页电量图标开机闪一下就没了（骑手动画每次重画都把它擦掉）→ 骑手画完补画蓝牙点和电量；
//         电量变化时只重画状态栏，不再整屏重画；偶尔读不到电池不会马上隐藏（连续 3 次才算没接电池）；
//         小地图上电量图标加黑色底，浅色地图上也看得清
//   v7.3  路程进度：屏幕左边沿一条细弧（在转弯进度环里面），从左下往左上走满 = 到达；白点是你现在的位置，
//         上端粉点是终点。用 "#e" 里的剩余里程自己算：每次剩余变少就记进“已走”，进度 = 已走 /（已走 + 剩余），
//         偏航重新规划也不会归零。"#M1" / "#M0" 开关（不带逗号，旧固件忽略；默认开）
//   v7.4  路程进度改到最外圈：一开始是一整圈绿环，从正上方顺时针，走过的部分变灰，交界处一个小白点；
//         快到路口出现转弯倒计时环时，倒计时环盖掉行程环，转完弯再回到行程环。去掉 v7.3 左边那条细弧
//   v7.5  行程环改细：8 像素 → 4 像素
//   v7.6  巡航模式不画路线：以前会把上一次导航留下的路线（粉色 / 走过变灰）、弯道上色、路况、红绿灯、终点标记画出来，
//         现在巡航时道路都保持地图原色
//   v7.7  圆屏转到 0/90/180/270 以外的角度时字发糊：推屏时改成双线性插值（取周围 4 个点按距离混合），
//         斜着转的字和线条边缘平滑、粗细均匀
//
// ---------------- 通信协议（手机 → 圆屏，每条消息一个蓝牙数据包） ----------------
//   R,300 / L,120 / S,800 / SL / SR / U / A   导航：方向,距离(米)
//   L,120,-135            v1.8：可选第三项 = 路口实际转弯角度（负左正右，单位度）
//   C                     清屏回到待机
//   P                     心跳
//   #v,42                 车速 km/h
//   #k,60,300 / #k        前方测速：限速,距离(米)；只有 #k 表示没有测速
//   #j,0033F0             车道：每条车道两个十六进制字符（背景方向码、推荐方向码，F=不推荐）；只有 #j 表示隐藏
//   #b,200                屏幕亮度 0~255
//   'B' + 二进制头 + 数据   文字图片（4 位灰度），见 handleBitmap()
//   #p,坐标系,x分米,y分米,朝向,车速   v2.0 当前位置（平面坐标，x 向东 y 向北）
//   #m,1 / #m,0           v2.0 小地图开 / 关
//   #q,40 / #q,60 / #q,80 v2.3 屏幕数据线速度（MHz），保存后重启生效
//   #f,1 / #f,0           v2.6 快速刷屏 开 / 关，保存后重启生效
//   #t,0 / #t,1 / #t,2    v2.9 地图配色：0 原版，1 罪恶都市，2 GTA5（v6.1 增加 3~10，见版本记录）
//   #Q,1 / #Q,0           v6.2 弯道急缓上色 开 / 关（v6.1 误用了 #q）
//   #R,0~3[,角度]         v7.10.1 兼容四档方向，带第二项时按精确角度旋转
//   #D角度                v6.4 圆屏方向任意角度 0~359（例 "#D37"；故意不带逗号，旧固件收到会忽略）
//   #UNPAIR               v6.5 忘掉配对过的手机（v6.6 起保留正在连着的这台）
//   #L毫秒                v6.7 小地图预判提前量 0~800（例 "#L300"；不带逗号，旧固件会忽略）
//   #n,1 / #n,0           v3.0 夜间配色 开 / 关
//   #r,1 / #r,2           v3.0 走过的路线：1 变灰，2 擦除
//   #e,12300,1432,8 / #e  v3.1 行程信息：剩余米,到达时间(时分),红绿灯数；只有 #e 表示隐藏
//   #g,120 / #g           v3.1 前方红绿灯距离（米）；只有 #g 表示隐藏
//   #c,坐标系,x,y,限速,图标,... / #c   v3.5 前方电子眼位置（米），最多 6 个；只有 #c 表示清空
//                         v3.8 最后一项是图标编号 0~15（手机按高德类型换算好，见 CAM_ICONS）
//   'M' + 二进制头 + 数据   v2.0 大块数据（1 = 地图块，2 = 导航路线，3 = 天地图图片地图，4 = 原路线 v4.4），见 handleBulk()
//   #o                    v4.4 不显示原路线
//   #z,1 / #z,0           v4.8 巡航模式开 / 关
//   #w,1 / #w,0           v4.9 清爽模式开 / 关
//   #i,1,号码 / #i,0      v5.1 来电显示开始 / 结束（名字用 'B' 图片栏 2）
//   #u,起,止,状态,...      v5.0 路况（米，离路线起点）；#y,1/0 重新规划中；#h,分钟 / #h 更快路线；#x,1/0 GPS 弱
//   #HM,0/1               v7.9 App 音乐控制 / HID 媒体按键（保存到圆屏；互斥）
//   HID,模式,可发按键        v7.9 HID 就绪状态；CAP,HID,1 能力通知
//   #Z,有会话,播放中,按钮掩码   v7.8 音乐状态（1=上一首，2=播放暂停，4=下一首）；图片栏 5/6 为歌名/歌手
// ---------------- 圆屏 → 手机（通知） ----------------
//   T,M,Q / T,M,P / T,M,T / T,M,N   v7.8 请求音乐状态 / 上一首 / 播放暂停 / 下一首
//   K,类型,编号           v2.0 这一份大块数据已收齐
//   U,1                   v6.5 这台手机通过了配对验证（连上后发 3 次）
//   V,电量,充电,USB,mV    v6.8 圆屏电池（电量 -1 = 没接电池）

#define FW_VERSION "v7.12.3"

// 调试时想在串口监视器里看到每条收到的文字指令，把 0 改成 1
#define DEBUG_SERIAL 0

// 导航中超过这么久没收到任何消息（含心跳），就提示数据中断
#define STALE_MS 6000

// 三阶段的分界距离（米），可按需要调整
#define STAGE_FAR_M  500   // 超过这个距离：远处预告画面
#define STAGE_NEAR_M 100   // 小于等于这个距离：橙色警示

#include <Arduino.h>
#include <nvs_flash.h>
#include "Arduino_GFX_Library.h"
#include "pin_config.h"
#include "BoardStopWatchInit.h"   // StopWatch：M5IOE1 扩展芯片（屏供电/复位/触摸复位）
#include "RtcRX8130.h"            // StopWatch：RX8130CE RTC 离线精准时钟
#include "LeanAngleMeter.h"       // StopWatch：BMI270 摩托车倾角与 G 值仪表
#include <Wire.h>
#include "TouchDrvCSTXXX.hpp"     // v5.2 触摸屏（SensorLib 库，微雪示例里带的）
#include <Preferences.h>
#include <esp_sleep.h>
#include <esp_system.h>
#include <driver/rtc_io.h>
#include "IdlePowerOff.h"
#include <driver/spi_master.h>
#include <esp_bt.h>
#include <esp_bt_main.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <BLE2902.h>
#include <BLEHIDDevice.h>
#include "HidMediaKeys.h"
#include "AppleMediaClient.h"
#include "ScreenRotationCommand.h"
#include <BLESecurity.h>       // v6.5 配对验证
#include <miniz.h>            // 罪恶都市轮廓：仅接收地图时解压
#include "BootAnimation.h"
#include "DragonRadar.h"
#include "TurnRingMotion.h"
#include "TrafficLightLab.h"
TurnRingMotion turnRingMotion;
uint32_t ringAnimationDrawAt = 0;
TrafficLightFrame trafficLightFrame;
TrafficLightLab trafficLightLab;
uint32_t trafficLightPreviewUntil = 0;
int bootBrand = 0;
String pendingBootAck;
#include <esp_mac.h>           // v7.0 读蓝牙地址，拼蓝牙名
#if defined(CONFIG_NIMBLE_ENABLED)
#include "host/ble_gap.h"     // v4.5 蓝牙 2M 模式
#include "host/ble_store.h"   // v6.5 记住的手机
#endif

// ===== 屏幕（和官方 HelloWorld 示例一样的初始化方式）=====
// v2.6 第 7 个参数 true = 总线可共享（快速刷屏要和它轮流使用数据线）
Arduino_DataBus *bus = new Arduino_ESP32QSPI(
  LCD_CS, LCD_SCLK, LCD_SDIO0, LCD_SDIO1, LCD_SDIO2, LCD_SDIO3, true);
Arduino_CO5300 *panel = new Arduino_CO5300(
  bus, LCD_RESET, 0, LCD_WIDTH, LCD_HEIGHT, 6, 0, 0, 0);
// 先画在内存里的“画布”上，画完再一次性推到屏幕（需要 PSRAM：OPI PSRAM）
// v2.2：可以切换画布内存的画布，用来做双缓冲
class SwapCanvas : public Arduino_Canvas {
public:
  SwapCanvas(int16_t w, int16_t h, Arduino_G *out) : Arduino_Canvas(w, h, out) {}
  void setFramebuffer(uint16_t *f) { _framebuffer = f; }
};
SwapCanvas *gfx = new SwapCanvas(LCD_WIDTH, LCD_HEIGHT, panel);
bool needFlush = false;
bool frameIsMap = false;          // v2.2 这一帧是小地图（整帧重画，可以异步刷屏）
int qspiMHz = 40;                 // v2.3 屏幕数据线速度
bool fastFlushOn = true;          // v2.6 快速刷屏

#include "CompanionMode.h"        // StopWatch：伴侣模式（KK 表情引擎与互动）


#include "GlobalConfig.h"

#include "BleManager.h"

#include "DisplayDriver.h"

#include "NavWidgets.h"

#include "PowerManager.h"

#include "DashboardWidgets.h"

#include "WaitingScreen.h"

#include "PairScreen.h"

#include "ArriveFx.h"

#include "TouchInput.h"

#include "CallScreen.h"

#include "WeatherPage.h"

// ---- v5.5 跟手换页 ----
// 触摸坐标和画面方向相反时改成 1（拖动时画面往手指反方向走就是反了）
// StopWatch：CST820B 与屏幕原生对齐（USB 朝下、不旋转），实测微雪的 180° 校正
// 反而把点按拧到了对面象限（音乐页按钮要点对角才生效）——两个翻转都关掉
#define TOUCH_FLIP_X 0      // v5.5.1 实测左右是反的
#define TOUCH_FLIP_Y 0      // 实机坐标确认：上下也反向；点按和手势使用同一校正
#define DRAG_START  16        // 手指移动超过这么多像素才开始拖
uint16_t *slideOld = nullptr, *slideNew = nullptr;
int16_t chordHalf[LCD_HEIGHT];   // 每一行在圆里的半宽（圆外不用算）
bool chordReady = false;
int lastSlideSx = -1, lastSlideSy = 0;      // 上次进天气页的方向（自动回去时反着滑）
void drawNavFrame();

#include "MusicPage.h"

#include "PageLeanMeter.h"

}

#include "PageRouter.h"

// ---- 进度环动画 ----
float ringShown = 0, ringTarget = 0;
bool ringActive = false;
String ringDir = "";

#include "MapEngine.h"

#include "PageNavigation.h"

#include "CommandHandler.h"

// v7.0 蓝牙名 = "MotoNav-" + 蓝牙地址最后两个字节
void makeBleName() {
  uint8_t mac[6] = {0};
  if (esp_read_mac(mac, ESP_MAC_BT) != ESP_OK) return;
  char b[20];
  snprintf(b, sizeof(b), "%s-%02X%02X", DEVICE_NAME, mac[4], mac[5]);
  bleName = b;
}

void setup() {
  makeBleName();                                       // v7.0 先算好蓝牙名（首页要显示）
  Serial.begin(115200);
  Serial.setTxTimeoutMs(0);   // 没打开串口监视器时打印不等待（v1.2 的修复）
  esp_err_t nvsRet = nvs_flash_init();
  if (nvsRet == ESP_ERR_NVS_NO_FREE_PAGES || nvsRet == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    nvs_flash_erase();
    nvs_flash_init();
  }
  Serial.printf("Reset reason=%d, sleep wake=%d\n", (int)esp_reset_reason(),
                (int)esp_sleep_get_wakeup_cause());
  Wire.begin(IIC_SDA, IIC_SCL);
  StopWatchBoard::init();   // StopWatch：M5IOE1 上电（屏电源/OLED复位/触摸复位），必须先于 touchInit/gfx->begin
  RtcRX8130::begin();
  RtcRX8130::syncToSystemTime();
  LeanAngleMeter::begin();
  touchInit();                                         // v5.2 触摸屏
  batInit();                                           // v6.8 电源芯片（电量）
  pinMode(BTN_A, INPUT_PULLUP);                        // StopWatch：用户按键 A/B（上拉、按下为低）
  pinMode(BTN_B, INPUT_PULLUP);

  // v2.3 读取保存的屏幕数据线速度（默认 40MHz，和官方示例一样）
  {
    Preferences pr;
    pr.begin("motonav", true);
    qspiMHz = pr.getInt("qspi", 40);
    screenRot = pr.getInt("rot", 0) & 3;                            // v6.2 圆屏方向
    setScreenDeg(pr.getInt("rotd", screenRot * 90), false);         // v6.4 任意角度
    bootBrand = BootAnimation::valid(pr.getInt("bootbrand", 0));
    hidMusicMode = pr.getBool("mediahid", false);
    fastFlushOn = pr.getBool("fastf", true);
    pr.end();
    if (qspiMHz != 40 && qspiMHz != 60 && qspiMHz != 80) qspiMHz = 40;
  }
  if (!gfx->begin(qspiMHz * 1000000)) Serial.println("gfx->begin() failed!");
  panel->setBrightness(200);

  iconBuf = (uint16_t *)ps_malloc(ICON_BUF_W * ICON_BUF_H * 2);   // v1.8.1 图标缓存
  buildRingLut();                                                   // v1.8.3 抗锯齿圆环查找表
  mapInit();                                                        // v2.0 小地图缓冲
  for (int i = 0; i < IMG_SLOTS; i++) {
    imgs[i].show = (uint8_t *)ps_malloc(IMG_MAX);
    imgs[i].recv = (uint8_t *)ps_malloc(IMG_MAX);
    imgs[i].showW = 0; imgs[i].showH = 0; imgs[i].recvId = -1;
  }

  // Begin with black instead of briefly displaying the waiting page before the intro.
  gfx->fillScreen(0);
  gfx->flush();
  initRotSpan();                                                    // v6.4 任意角度用
  initDoubleBuffer();                                               // v2.2 双缓冲 + 核心 0 刷屏
  helperInit();                                                     // v5.7 另一个核心帮忙画底图

  if (bootBrand != BootAnimation::Off && gfx->getFramebuffer()) {
    uint32_t bootAt = millis();
    while (millis() - bootAt < BootAnimation::Duration) {
      uint32_t frameAt = millis();
      BootAnimation::render(gfx->getFramebuffer(), frameAt - bootAt, bootBrand);
      flushSync();
      uint32_t spent = millis() - frameAt;
      delay(spent < 33 ? 33 - spent : 1);
    }
  }
  drawWaiting(WAIT_NOLINK);
  flushSync();
  needFlush = false;

  BLEDevice::init(bleName);                    // v7.0 名字带地址后 4 位；名字放不进广播包时库会自动放到扫描应答里
  BLEDevice::setMTU(517);     // 允许手机协商大数据包，用来传文字图片
#if defined(CONFIG_NIMBLE_ENABLED)
  // v4.5 允许蓝牙 2M 模式（速度翻倍，手机请求时直接同意）和最长空中数据包（251 字节）
  ble_gap_set_prefered_default_le_phy(BLE_GAP_LE_PHY_1M_MASK | BLE_GAP_LE_PHY_2M_MASK,
                                      BLE_GAP_LE_PHY_1M_MASK | BLE_GAP_LE_PHY_2M_MASK);
  ble_gap_write_sugg_def_data_len(251, 2120);
#endif
  // v6.5 配对验证：圆屏显示 6 位随机配对码（每次配对都换），手机输入；要绑定 + 防中间人 + 安全连接
  {
    BLESecurity *sec = new BLESecurity();                 // 构造时设好“交换加密密钥和身份密钥”（手机换地址也认得出）
    sec->setCapability(ESP_IO_CAP_OUT);                   // 圆屏只能显示，不能输入
#if defined(CONFIG_NIMBLE_ENABLED)
    sec->setAuthenticationMode(true, true, true);         // 绑定、防中间人、安全连接
    sec->setPassKey(false);                               // 随机配对码
    sec->regenPassKeyOnConnect(true);                     // 每次配对重新随机
#else
    sec->setAuthenticationMode(ESP_LE_AUTH_REQ_SC_MITM_BOND);
#endif
    BLEDevice::setSecurityCallbacks(new SecCB());
  }
  BLEServer *server = BLEDevice::createServer();
  server->setCallbacks(new ServerCB());
  BLEService *svc = server->createService(SERVICE_UUID);
  BLECharacteristic *rx = svc->createCharacteristic(
    RX_UUID, BLECharacteristic::PROPERTY_WRITE | BLECharacteristic::PROPERTY_WRITE_NR
#if defined(CONFIG_NIMBLE_ENABLED)
             | BLECharacteristic::PROPERTY_WRITE_ENC | BLECharacteristic::PROPERTY_WRITE_AUTHEN   // v6.5 必须配对过才能写
#endif
  );
  rx->setCallbacks(new RxCB());
  txChar = svc->createCharacteristic(TX_UUID, BLECharacteristic::PROPERTY_NOTIFY);   // v2.0
  txChar->addDescriptor(new BLE2902());
  svc->start();
  initMediaHid(server);
  appleMedia.begin();
  BLEAdvertising *adv = BLEDevice::getAdvertising();
  adv->addServiceUUID(SERVICE_UUID);
  adv->addServiceUUID(BLEUUID((uint16_t)0x1812));
  adv->setAppearance(GENERIC_HID);
  adv->setScanResponse(true);
  BLEDevice::startAdvertising();
  idlePowerOffReset(&idlePowerOffTimer, millis());
  Serial.println(String("MotoNav ready, firmware ") + FW_VERSION);
}

// Power down before destroying BLE objects: the host can still dispatch events
// referencing those objects. Shutdown does not need to reclaim their memory.
void idlePowerOffShutdown() {
  powerOffPending = true;
  Serial.println("No navigation for 5 minutes: powering off");
  const bool displayReady = !flushDone ||
      xSemaphoreTake(flushDone, pdMS_TO_TICKS(3000)) == pdTRUE;
  if (displayReady) {
    panel->setBrightness(0);
    panel->displayOff();
  }
  // StopWatch: 彻底切断 AMOLED 屏 3.3V 供电 (L3B / M5IOE1 PIN_8)
  // 消除待机微安级以外的显示屏静态漏电，大幅延长关机休眠续航
  if (StopWatchBoard::ok) {
    StopWatchBoard::ioe.digitalWrite(M5IOE1_PIN_8, 0);
  }
  // StopWatch：真关机走 M5PM1（0x6E）SYS_CMD=OFF，之后按 PWR 键重新上电。
  // AXP2101(0x34) 掉电分支是 175 板遗留；StopWatch 板上 0x34 没有设备，永远读不到 0x4A。
  if (StopWatchBoard::pm1OK) {
    const m5pm1_err_t result = StopWatchBoard::pm1.shutdown();
    Serial.printf("Shutdown: PM1 result=%d\n", (int)result);
    // 真断电则硬件掉电停止执行；能走到这里说明 PM1 不可用或外部 USB 供电还在。
    if (result == M5PM1_OK) delay(1500);
  }
  // Check the chip ID, rather than writing to an unknown device at address 0x34.
  const int chip = pmuRead(0x03);
  Serial.printf("Shutdown: PMIC id=%02X, on=%02X, off=%02X\n",
                chip, pmuRead(0x20), pmuRead(0x21));
  if (chip == 0x4A) {
    const int config = pmuRead(0x10);
    if (config >= 0) {
      Wire.beginTransmission(PMU_ADDR);
      Wire.write(0x10);
      // Bit 1 is a separate, self-clearing RESET command; never replay it.
      Wire.write((uint8_t)((config & ~0x02) | 0x01));
      const int result = Wire.endTransmission();
      Serial.printf("Shutdown: PMIC command result=%d\n", result);
      delay(1000);
    }
  }
  // If the PMIC is unavailable or external power keeps the CPU supplied, stop
  // screen/BLE/CPU activity instead of repeatedly rebooting or staying awake.
  // Normal PMIC-off wakes via PWR; this fallback can also wake via BOOT (GPIO0).
  Serial.println("PMIC did not remove CPU power; entering deep sleep");
  // Stop the host/controller without deleting live GATT objects. Deep sleep
  // discards RAM anyway. Keep objects valid until pending callbacks have ended.
#if defined(CONFIG_NIMBLE_ENABLED)
  nimble_port_stop();
#elif defined(CONFIG_BLUEDROID_ENABLED)
  esp_bluedroid_disable();
#endif
#if CONFIG_BT_CONTROLLER_ENABLED
  esp_bt_controller_disable();
#endif
  esp_sleep_disable_wakeup_source(ESP_SLEEP_WAKEUP_ALL);
  // StopWatch：唤醒源 = 两个用户按键（A=GPIO2、B=GPIO1，上拉、按下为低），
  // 任意一个按下即唤醒（EXT1 任意低电平）。原版的 GPIO0/BOOT 在这块板上没有引出。
  {
    const gpio_num_t wakeKeys[2] = { GPIO_NUM_1, GPIO_NUM_2 };
    for (int i = 0; i < 2; i++) {
      rtc_gpio_init(wakeKeys[i]);
      rtc_gpio_set_direction(wakeKeys[i], RTC_GPIO_MODE_INPUT_ONLY);
      rtc_gpio_pullup_en(wakeKeys[i]);
      rtc_gpio_pulldown_dis(wakeKeys[i]);
    }
    // EXT1 低电平触发：按着键入睡会导致立刻唤醒重启，等两键都松开 100ms 再武装
    uint32_t releasedAt = millis(), waitAt = releasedAt;
    while (millis() - waitAt < 1500 && millis() - releasedAt < 100) {
      if (!rtc_gpio_get_level(GPIO_NUM_1) || !rtc_gpio_get_level(GPIO_NUM_2)) releasedAt = millis();
      delay(10);
    }
    if (millis() - releasedAt >= 100) {
      esp_sleep_enable_ext1_wakeup((1ULL << 1) | (1ULL << 2), ESP_EXT1_WAKEUP_ANY_LOW);
    } else {
      Serial.println("key held; wake disabled (use PWR to power cycle)");
    }
  }
  esp_deep_sleep_start();
}

void loop() {
  hidMediaTick();                                      // press/release without blocking navigation
  static uint32_t lyricConnectionEpoch = 0;
  if (lyricConnectionEpoch != mediaConnectionEpoch) {
    lyricConnectionEpoch = mediaConnectionEpoch; musicLyrics = false; lyricImageGeneration = 0;
    imgs[12].showW = 0; imgs[12].showH = 0; imgs[12].recvId = -1; musicDirty = true;
  }
  appleMedia.tick(bleConnected && bleAuthed && hidMusicMode, bleConnHandle, mediaConnectionEpoch, millis());
  static int lastControlMask = -1;
  int controlMask = appleMedia.controlMask();
  if (lastControlMask != controlMask) { lastControlMask = controlMask; pendingMediaStatus = String("AMC,") + controlMask; musicDirty = true; Serial.printf("MEDIA AMS controls=%d\n", controlMask); }
  if (appleMedia.controlAck) { musicAck = appleMedia.controlAck > 0 ? 9 : 10; appleMedia.controlAck = 0; musicAckUntil = millis() + 2500; musicDirty = true; }
  if (appleMedia.changed) {
    appleMedia.changed = false; musicDirty = true;
    static uint32_t lyricGeneration = 0; static uint16_t lyricTitleRevision = 0, lyricArtistRevision = 0;
    if (lyricGeneration != appleMedia.generation || lyricTitleRevision != appleMedia.values[0].revision || lyricArtistRevision != appleMedia.values[1].revision) {
      lyricImageGeneration = 0;
      lyricGeneration = appleMedia.generation; lyricTitleRevision = appleMedia.values[0].revision; lyricArtistRevision = appleMedia.values[1].revision;
      imgs[12].showW = 0; imgs[12].showH = 0; imgs[12].recvId = -1;
    }
    if (hidMusicMode) { musicHave = appleMedia.available && appleMedia.values[0].text.length(); musicPlaying = appleMedia.playback.known && appleMedia.playback.state == 1; }
  }
  static unsigned long mediaDrawAt = 0;
  if (hidMusicMode && appleMedia.available && millis() - mediaDrawAt >= 1000) { mediaDrawAt = millis(); musicDirty = true; }
  saveRotIfDue();                                      // v6.4
  // v6.5 连上 60 秒还没通过验证（不是自己的手机）：断开，让自己的手机能连
#if defined(CONFIG_NIMBLE_ENABLED)
  if (bleConnected && !bleAuthed && bleConnHandle != 0xFFFF && millis() - bleConnAt > AUTH_TIMEOUT_MS) {
    ble_gap_terminate(bleConnHandle, BLE_ERR_REM_USER_CONN_TERM);
    bleConnAt = millis();                              // 别每圈都断
  }
#endif
  // 1. 连接状态变化（先处理）；v6.5 通过验证才算连上
  bool conn = bleConnected && bleAuthed;
  if (conn != shownConn) {
    shownConn = conn;
    if (conn) {
      lastRxMs = millis();
      drawWaiting(WAIT_READY);
    } else {
      bool wasNav = inNav;
      resetNavState();
      staleShown = false;
      drawWaiting(wasNav ? WAIT_LOST : WAIT_NOLINK);   // 导航途中断开 → 红色 LINK LOST
    }
  }

  // 2. 取出队列里的所有消息逐条处理（最多 30 条，避免卡住画面）
  static RxMsg local;
  for (int i = 0; i < 30; i++) {
    bool got = false;
    portENTER_CRITICAL(&mux);
    if (rxTail != rxHead) {
      local.len = rxq[rxTail].len;
      memcpy(local.data, rxq[rxTail].data, local.len);
      rxTail = (rxTail + 1) % RXQ_SLOTS;
      got = true;
    }
    portEXIT_CRITICAL(&mux);
    if (!got) break;
    handleMessage(local.data, local.len);
  }

  updateStats();
  if (pendingStat.length()) { pendingStat2 = pendingStat; pendingStat = ""; }
  sendPendingAck();                                    // v2.0 地图收齐的确认 / v2.2 帧率

  // 3. 进度环动画；小地图模式下连续刷新（v2.2 不再限制帧间隔）
  animateRing();
  // 地图帧原本连续刷新；大箭头画面也按现有帧节奏推进行程环。
  static unsigned long tripDrawAt=0;
  static float tripDrawValue=-1;
  float tripValue=tripFrac();
  if(inNav && progOn && tripOn && !cruiseOn && !ringActive && !staleShown &&
     tripValue>=0 && tripValue!=tripDrawValue && millis()-tripDrawAt>=33) {
    tripDrawAt=millis();tripDrawValue=tripValue;navDirty=true;
  }
  if(tripValue<0)tripDrawValue=-1;
  static bool wasMap = false;
  bool mapNow = inNav && mapModeActive();
  if (mapNow != wasMap) { wasMap = mapNow; navDirty = true; }   // 进出地图模式时整帧重画
  if (mapNow) navDirty = true;

  touchPoll();                                         // v5.2 按住接听 / v5.3 滑动换页
  buttonTick();                                        // StopWatch：用户按键换页
  batPoll();                                           // v6.8 电量
  // 4. 需要时重画导航画面
  // v6.5 配对：整屏显示配对码，配好 / 失败 / 60 秒 / 断开后回到原来的画面
  if (pairReq) {
    pairReq = false;
    if (sliding) dragAbort();
    pairOn = true; pairShowAt = millis();
    drawPairScreen();
  }
  if (pairOn && (pairDone || millis() - pairShowAt > AUTH_TIMEOUT_MS || !bleConnected)) {
    pairOn = false; pairDone = false;
    if (page == 2) musicDirty = true;
    else if (page == 1) wxDirty = true;
    else if (page == 3) leanDirty = true;
    else if (inNav) navDirty = true;
    else drawWaiting(conn ? WAIT_READY : WAIT_NOLINK);
  }
  // v5.1 来电：整屏显示（约 12 帧/秒的动画），结束后回到原来的画面
  static bool wasCall = false;
  if (callOn && (millis() - callSeenAt > 120000 || !conn)) callOn = false;      // 2 分钟没消息 / 断开：收起
  if (callOn && sliding) dragAbort();                  // v5.5 拖到一半来电：放弃换页
  if (callOn) {
    static unsigned long lastCallDraw = 0;
    if (millis() - lastCallDraw > 80) { lastCallDraw = millis(); drawCallScreen(); }
    wasCall = true;
  } else if (wasCall) {
    wasCall = false;
    if (page == 2) musicDirty = true;
    else if (page == 1) wxDirty = true;
    else if (page == 3) leanDirty = true;
    else if (inNav) navDirty = true;
    else drawWaiting(conn ? WAIT_READY : WAIT_NOLINK);
  }

  // v7.1 到达特效：来电、配对优先；放完 / 被点掉后回到原来的画面
  static bool wasFx = false;
  if (fxOn && (millis() - fxAt > fxDur || !conn)) fxOn = false;
  if (fxOn) {
    static unsigned long lastFxDraw = 0;
    if (!callOn && !pairOn && page == 0 && !sliding && millis() - lastFxDraw >= 33) {
      lastFxDraw = millis();
      drawArriveFx(millis() - fxAt);
    }
    wasFx = true;
  } else if (wasFx) {
    wasFx = false;
    if (!callOn && !pairOn) {
      if (page == 2) musicDirty = true;
      else if (page == 1) wxDirty = true;
      else if (page == 3) leanDirty = true;
      else if (inNav) navDirty = true;
      else drawWaiting(conn ? WAIT_READY : WAIT_NOLINK);
    }
  }

  musicTick();
  weatherTick();                                       // v5.3 天气页
  leanTick();                                          // StopWatch：倾角与 G 值仪表页
  CompanionMode::updateVibration(millis());            // 全局震动定时调度

  // 伴侣模式每帧渲染与超时调度
  if (page == 4 && !sliding && !callOn && !pairOn) {
    CompanionMode::tick();

    const uint32_t now = millis();
    const uint32_t idleMs = now - CompanionMode::lastInteractionAtMs;

    if (inNav) {
      // 导航过程中进入伴侣模式：1 分钟无互动才返回导航界面；导航过程中永不休眠
      if (idleMs >= 60000UL) {
        Serial.println("[Companion] In navigation: 1 min idle -> Return to navigation");
        showPageAnimated(0, 0, 1);
      }
    } else {
      // 非导航状态：
      idlePowerPollSupply();
      const bool isExtPower = idlePowerSupply.sampled && !idlePowerSupply.batteryOnly;
      if (!isExtPower) {
        // 电池待机情况下：5 分钟无互动操作就进入休眠
        if (idleMs >= 300000UL) {
          Serial.println("[Companion] Battery standby: 5 min idle -> Power off/Deep sleep");
          idlePowerOffShutdown();
        }
      }
      // 接电情况下：永不休眠，持续运行伴侣表情
    }
  }

  // 手机 App 连接成功后自动跳到主程序界面（仅非导航待机下触发）
  static bool wasBleAuthed = bleConnected && bleAuthed;
  bool authedNow = bleConnected && bleAuthed;
  if (!inNav && authedNow && !wasBleAuthed) {
    if (page == 4 && !sliding) {
      Serial.println("[Companion] Phone connected & authed -> Return to home screen");
      showPageAnimated(0, 0, 1);
    }
  }
  wasBleAuthed = authedNow;

  // 主程序界面待机：2 分钟内无连接且无操作自动进入伴侣模式（电池与接电均适用）
  static unsigned long homeNoConnSince = 0;
  const unsigned long loopNow = millis();
  if (page == 0 && !inNav && !bleConnected && !sliding && !callOn && !pairOn && !fxOn && !touchDown && !dragging) {
    if (homeNoConnSince == 0) homeNoConnSince = loopNow;
    if (loopNow - homeNoConnSince >= 120000UL) {
      homeNoConnSince = loopNow;
      Serial.println("[Companion] 2 min unconnected on home screen -> Auto enter avatar mode");
      showPageAnimated(4, 0, 1);
    }
  } else {
    homeNoConnSince = loopNow;
  }

  if (inNav && navDirty && !fxOn && !callOn && !pairOn && page == 0 && !sliding) {
    drawNavFrame();
    navDirty = false;
  }

  // 5. 导航中长时间收不到任何消息 → 数据中断提示
  if (inNav && conn && !fxOn && !callOn && !pairOn && page == 0 && !sliding && !staleShown && millis() - lastRxMs > STALE_MS) {
    staleShown = true;
    drawStaleOverlay();
  }

  // 红绿灯覆盖层实时调度（对齐微雪 1.75 版）
  const uint32_t lightNow = millis();
  if (!conn || !trafficLightLab.valid(lightNow)) trafficLightLab.clear();
  const bool lightPreview = trafficLightPreviewUntil && (int32_t)(trafficLightPreviewUntil - lightNow) > 0;
  if (!lightPreview) trafficLightPreviewUntil = 0;
  const bool showLight = (trafficLightLab.valid(lightNow) || trafficLightFrame.valid(lightNow) || lightPreview) && conn && page == 0 && !sliding && !pairOn && !callOn && !fxOn && !staleShown;
  static bool lightShown = false;
  static uint32_t lightDrawAt = 0;

  if (showLight) {
    if (!lightShown || needFlush || lightNow - lightDrawAt >= 200) {
      drawTrafficLightLab(lightNow);
      lightDrawAt = lightNow;
    }
  } else if (lightShown && !pairOn && !callOn && !fxOn && !sliding) {
    if (page == 2) musicDirty = true;
    else if (page == 1) wxDirty = true;
    else if (page == 3) leanDirty = true;
    else if (inNav) navDirty = true;
    else drawWaiting(conn ? WAIT_READY : WAIT_NOLINK);
  }
  if (lightShown != showLight) {
    Serial.printf("TL OVERLAY visible=%d inNav=%d at=%lu valid=%d conn=%d page=%d sliding=%d pair=%d call=%d fx=%d stale=%d\n",
      showLight, inNav, lightNow, trafficLightLab.valid(lightNow), conn, page, sliding, pairOn, callOn, fxOn, staleShown);
  }
  lightShown = showLight;

  animateCat();                                        // v3.6 首页猫猫动画

  // 6. 有变化才把画布推到屏幕（小地图帧异步推送，其他画面同步推送）
  if (needFlush && !sliding) {
    if (frameIsMap) flushAsync(); else flushSync();
    needFlush = false;
    frameIsMap = false;
  }
  idlePowerPollSupply();
  // 导航中永远不能休眠；伴侣模式有单独的 5 分钟超时休眠逻辑
  const bool navigating = inNav || cruiseOn || idlePowerOffNavigationActive(
      bleConnected && bleAuthed, inNav, cruiseOn, navDir == "A");
  const bool interacting = pairOn || pairReq || callOn || touchDown || dragging || sliding || (page == 4);
  // StopWatch：断连满 2 分钟真关机；配对/来电/滑页/伴侣模式均不触发
  const bool linked = bleConnected;
  const bool busy = pairOn || pairReq || callOn || sliding || (page == 4);
  const bool due = idleLinkOffDueOnBattery(&idlePowerOffTimer, millis(), linked, busy, &idlePowerSupply) ||
                   (!inNav && idlePowerOffDueOnBattery(&idlePowerOffTimer, millis(), navigating, interacting, &idlePowerSupply));
  if (due) {
    bool queued;
    portENTER_CRITICAL(&mux);
    queued = rxTail != rxHead;
    portEXIT_CRITICAL(&mux);
    if (!queued) {
      // Never decide from the 5-second battery UI cache. Re-read at the deadline.
      idlePowerPollSupply(true);
      if (idleLinkOffDueOnBattery(&idlePowerOffTimer, millis(), linked, busy, &idlePowerSupply) ||
          (!inNav && idlePowerOffDueOnBattery(&idlePowerOffTimer, millis(), navigating, interacting, &idlePowerSupply)))
        idlePowerOffShutdown();
    }
  }
  delay((mapNow || page == 4) ? 1 : 10);
}
