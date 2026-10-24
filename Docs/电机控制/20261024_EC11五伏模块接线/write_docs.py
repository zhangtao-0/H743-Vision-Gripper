from pathlib import Path
import json,hashlib,re,shutil,csv
H=Path(__file__).resolve().parents[3];E=Path(__file__).resolve().parent
text='''# EC11五伏旋钮模块接线与操作

用户确认：模块VCC为5V，C为按压。沿用已经分配的三路GPIO，不另占空闲资源。旋钮与电机自带霍尔编码器是两套不同输入，不能交叉接线。

## 按实际丝印接线

| 模块脚 | H743引脚 | 开发板排针 | 作用 |
|---|---|---|---|
| VCC | +5V | U9-3或U9-4 | 模块供电，用户已连接 |
| GND | GND | U10-1～4任选已确认地脚 | 公共地，用户已连接 |
| A | PG2 | U10-57 | 旋转A相，改变目标速度 |
| C | PC13 | U10-7 | KEY7按压：点按启停，长按1秒换向 |
| B | PG3 | U10-58 | 旋转B相，判断旋转方向 |

PC13、PG2、PG3在STM32H743ZI数据手册中分别标为FT、FT_h、FT_h，均是5V容限数字输入。本轮将这三路内部上下拉禁用（GPIO_NOPULL），因此可在模块由开发板5V供电、H743正常上电、输入为数字模式的当前接法下接入5V信号。不能把这一结论推广到其他GPIO或模拟输入。

**只在烧录本轮NOPULL修正版后接三根信号线。** 原版对旋钮启用了内部上拉，不满足数据手册“输入高于4V时禁用内部上下拉”的条件。模块C已经确认是按压输出，不是裸编码器公共端，不要把C直接接GND。

模块A/B/C需要有自身上拉/输出驱动，使开关松开时输入稳定；若模块没有上拉，在每路信号与模块5V之间各加10kΩ外部上拉。内部NOPULL保持不变。当前按压逻辑为按下低、松开高；用户确认了C功能，但本轮尚未测量C极性。若实物C按下为高，先保持停机，需调整输入极性，不能靠重复启停尝试。

5V模块供电取自H743板，二者一起上电；不要在H743断电时由其他5V电源继续向其信号脚送电。PG2/PG3为旋钮输入；电机霍尔A=PA8、B=PE11，保持上一轮接线不变。PB14仍接L298N ENA，不能接到PE13。

## 使用方式

旋转每格改变5RPM，范围0～400；默认400在上限，所以先往减速方向转，才能明显看到目标值变化。若增减方向与习惯相反，可以交换本旋钮A/B两根线（PG2/PG3），不要交换电机霍尔A/B。默认每定位点4个有效边沿；如果转两格才变化一次，需要核对旋钮2/4边沿规格，不修改电机测速CPR。

| 动作 | 电机行为 | 上位机等效输入 |
|---|---|---|
| 正/反向旋转一格 | 目标±5RPM | @knob 1 / @knob -1 |
| C点按并松开 | 启动/停机，停机保留目标 | @key 7 tap |
| C长按约1秒 | 换向一次，先滑行再反向 | @key 7 press 1200 |

它已经接入复用授权，不需要新增屏幕键：在屏幕DEBUG/KEY GRANTS中打开Motor keys，或者上位机逐行发送：

```text
@motor disable
@input mode mixed
@debug sts off
@debug motor on
@motor rpm 200
```

mixed接受实体与模拟输入；如果之前设成remote，实体旋钮和按压不会执行。Motor keys授权保持到复位或手动关闭，离开DEBUG页不会撤销；上电默认OFF。关闭授权不会主动停车，停车用@motor disable。KEY1～4仍操作屏幕；KEY5/KEY6的已设计复用行为不变。旋钮C为KEY7，目前仅在Motor keys授权下控制电机，不额外触发舵机动作。允许STS与Motor同时授权，但KEY5/6会广播到两套业务，单独调电机建议STS OFF。

需要VOFA曲线时，使用HOST USB-TTL串口（上一轮COM9）115200/8N1、JustFloat，发@motor plot on；CH0目标、CH1实际、CH2斜坡参考。只关绘图用@motor plot off，停止电机用@motor disable。绘图期间文字回复被抑制，命令仍接收。

## 实体接线后验收

1. 模块接线前先停机并关闭授权；确认本轮修正版已烧录。
2. A/C/B按上表接好，保留电机/L298N接线。
3. 机构无负载、可以自由运动时，执行上面的授权与200目标命令。
4. 旋转旋钮，应只改变目标值，尚未按C时电机保持停机；可发@motor status查看target。
5. 点按C，电机启动；旋转改变目标；再次点按C，停机。
6. 要验证换向，再启动后按住C约1秒，观察先滑行再反向；松开不会额外启停。
7. 测完发@motor disable、@debug motor off。

本轮不自动启动电机。上一轮已实测电机400RPM闭环、模拟旋钮和模拟KEY5启停；这不代表本轮实体EC11已经接线验收。C极性、每格边沿数、实际接线和手势仍需实体测试。

## 源码及依据

- [引脚与输入电阻配置](../../../project/App/platform/boards/lxb743zi_p1/board_devices.h)：BOARD_MOTOR_KNOB_PULL与BOARD_MOTOR_KEY_PULL集中配置为NOPULL。
- [GPIO初始化](../../../project/App/platform/board_motor.c)：BoardMotor_Init分别初始化PG2/PG3和PC13数字输入。
- [旋转、按压和授权业务](../../../project/App/modules/motor/motor_service.c)：旋转Gray码、20ms按压消抖、短按启停、1秒换向。
- [上一轮闭环实测](../20261024_L298N切换与实测/独立驱动诊断/README.md)。
- [ST原始数据手册](https://www.st.com/resource/en/datasheet/stm32h743zi.pdf)：PC13=FT，PG2/PG3=FT_h，输入高于4V禁用内部上下拉。工程已有[原文快照](../../../.firecrawl/20261024-stm32h743-pa8-af.md)。
'''
(E/'README.md').write_text(text,encoding='utf8')
with (E/'EC11接线表.csv').open('w',encoding='utf-8-sig',newline='') as f:
 w=csv.writer(f);w.writerow(['模块信号','MCU','开发板排针','功能','条件'])
 w.writerows([['VCC','+5V','U9-3或U9-4','模块供电','来自同一H743板'],['GND','GND','U10-1～4','共地',''],['A','PG2','U10-57','旋转A相','数字输入NOPULL，外部上拉/模块驱动'],['C','PC13','U10-7','KEY7按压','数字输入NOPULL，按下低有效待实物核对'],['B','PG3','U10-58','旋转B相','数字输入NOPULL，外部上拉/模块驱动']])
# Update source facts and all generated device/pin tables.
p=H/'Docs/引脚与接线/引脚资源.json';d=json.loads(p.read_text(encoding='utf8'));shutil.copy2(p,E/'引脚资源.before.json')
for pin in ['PG2','PG3','PC13']:
 r=d['roles'][pin];r['mode']='GPIO数字输入/NOPULL/5V容限；外部上拉或模块驱动'
 r['wiring']='用户确认模块VCC=5V、C为按压；A→PG2/U10-57、B→PG3/U10-58、C→PC13/U10-7。FT输入，内部上下拉禁用。C按下低有效待实测，不接公共地。Motor keys授权后旋转±5RPM、点按启停、长按1秒换向。'
d['roles']['PC13']['signal']='C/SW/KEY7（按压已确认，极性待实测）'
changed=['App/platform/board_motor.c','App/platform/boards/lxb743zi_p1/board_devices.h']
for rel in changed:
 assert rel in d['source_hash'];d['source_hash'][rel]=hashlib.sha256((H/'project'/rel).read_bytes()).hexdigest()
p.write_text(json.dumps(d,ensure_ascii=False,indent=2),encoding='utf8')
# Maintain reviewed teaching baseline and function references for the two changed source files only.
base=H/'Docs/教学手册/05_维护与证据';p=base/'固件源码基线.json';d=json.loads(p.read_text(encoding='utf-8-sig'));shutil.copy2(p,E/'教学基线.before.json')
for rel in changed:d[rel]=hashlib.sha256((H/'project'/rel).read_bytes()).hexdigest()
p.write_text(json.dumps(d,ensure_ascii=False,indent=2),encoding='utf8')
p=base/'源码函数定位.json';rows=json.loads(p.read_text(encoding='utf-8-sig'));shutil.copy2(p,E/'源码函数定位.before.json')
for r in rows:
 if r['path'] in changed:
  f=H/'project'/r['path'];lines=f.read_text(encoding='utf-8-sig').splitlines();hits=[i+1 for i,l in enumerate(lines) if re.search(r'\b'+re.escape(r['symbol'])+r'\s*\(',l)]
  assert len(hits)==1,(r,hits);r['line']=hits[0];r['source_sha256']=hashlib.sha256(f.read_bytes()).hexdigest()
p.write_text(json.dumps(rows,ensure_ascii=False,indent=2),encoding='utf8')
# Remove stale instructions that confused the module C output with a bare EC11 common contact.
p=H/'Docs/引脚与接线/01_按键接线与测试.md';s=p.read_text(encoding='utf8').replace('EC11旋转A/B为PG2/U10-57、PG3/U10-58，公共端接GND；旋转与PC13按压是三路不同输入。','当前五针5V模块：A→PG2/U10-57，B→PG3/U10-58，C按压→PC13/U10-7；GND单独共地，C不接地。三路NOPULL、外部上拉/模块驱动。详见[EC11五伏接线](../电机控制/20261024_EC11五伏模块接线/README.md)。');p.write_text(s,encoding='utf8')
p=H/'Docs/电机控制/04_VOFA旋钮按键与PID曲线.md';s=p.read_text(encoding='utf8');s=s.replace('当前没有厂家引脚资料证明C的功能，需确认按压时C是否改变电平；若是低有效按压输出，接PC13/U10-7（KEY7）。','用户已确认C为按压输出，接PC13/U10-7（KEY7），按下低有效极性待实物核对。');s=s.replace('5V模块先供5V，并测量/核实A/B/C输出电平，再接H743。','最新直接接线方案见[EC11五伏模块](20261024_EC11五伏模块接线/README.md)：PG2/PG3/PC13均已核对为FT输入，修正版禁用内部上下拉后可接当前5V模块信号。下面的电平缓冲方案仍是可选方式。');s=s.replace('未核实前不把5V输出直接接PC13，也不宣称该模块可以可靠用3.3V供电。','旧GPIO上拉版本不得直接接5V信号；本轮NOPULL修正版条件见最新接线方案。模块仍用5V供电。');p.write_text(s,encoding='utf8')
p=H/'Docs/电机控制/README.md';s=p.read_text(encoding='utf8');s=s.replace('\n- [VOFA旋钮','\n- [EC11五伏模块A/C/B直接接线与授权](20261024_EC11五伏模块接线/README.md)\n- [VOFA旋钮',1);p.write_text(s,encoding='utf8')
p=H/'Docs/电机控制/01_硬件参数与按设备接线.md';s=p.read_text(encoding='utf8');a=s.index('模块额定5V，五针');b=s.index('\n## 设备4',a);s=s[:a]+'''用户确认模块5V供电、C为按压。A→PG2/U10-57、B→PG3/U10-58、C→PC13/U10-7（KEY7）。三脚均为FT输入，本轮修正版内部上下拉禁用，可接当前5V模块信号，依赖外部上拉或模块驱动。C不是公共端，不接GND。完整名单和使用方法见[EC11五伏模块](20261024_EC11五伏模块接线/README.md)。

默认每格5RPM、0～400；点按启停、长按1秒换向，长按释放不额外启停。必须开启Motor keys授权；physical/mixed模式接受实体输入。每格2/4边沿、C按下低有效仍待实体测试。
''' + s[b:];p.write_text(s,encoding='utf8')
print('Saved wiring CSV/manual, updated pin facts and removed contradictory C-to-GND instruction')
