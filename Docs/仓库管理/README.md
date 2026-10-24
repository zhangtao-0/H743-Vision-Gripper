# H743独立Git仓库与上传范围

仓库根目录就是H743，不包含父目录的整个软硬件资料库。源码、工程文件、当前项目需要的SDK、测试、文档、PC程序及视觉配置/素材清单纳入版本控制。保留SDK原有许可文件；没有把整个项目擅自改为另一种统一开源许可证。

## 保留与忽略

- 保留project/App、Core、Drivers、Middlewares、USB_DEVICE、.ioc、IAR .ewp/.eww/.ewd、启动汇编和链接脚本。
- 保留Docs、Tools、Tests、Host与Vision_AI的源代码、依赖清单、配置、清单及轻量评估资料。
- 忽略IAR Obj/Exe/List/BrowseInfo/settings、固件HEX/BIN、对象文件、MAP/日志/调试数据库；Docs中的同类生成产物也忽略。
- 忽略Python虚拟环境/缓存、模型权重和自动训练输出。素材图片目录按Vision_AI已有.gitignore保持本地，下载来源/标签/版本清单继续上传。
- 忽略Backups、.firecrawl、Docs/官方参考历程。官方原始例程约1.35GB，当前已适配的代码在project/App/vendor，活动SDK在project/Drivers；忽略原件不会移除活动构建所需代码。
- 保留SDK随包的.a/.lib依赖，它们并非本工程的构建输出，不能一概忽略。

.gitignore不删除本机文件。之后新建构建目录时，增加对应目录规则。必要时执行git check-ignore -v 文件路径核对具体规则。

## 获取和构建

克隆后打开project/EWARM/Project.eww，选择H743_VisionGripper、H743_VisionGripper_OV2640或H743_VisionGripper_NoCamera顺序构建。需要本机安装匹配的IAR和开发环境，工具路径按自己的安装位置配置。构建产物在本机重新生成，不从仓库直接使用历史HEX。

Host/vision_gripper和Vision_AI的Python环境按各自README及requirements建立；Git仓库不携带.venv和模型权重。素材恢复以Vision_AI/Library的来源和版本清单为准，不自动上传本机整个图片库。

H743范围之外的总知识库、原理图与原始厂商资料不在仓库内。因此部分历史文档的本机路径、向上跨目录链接、历史备份核验和原理图核验工具需要原资料库才能完整运行；普通IAR源码构建不依赖父资料库。被忽略的HEX、BIN、日志和备份相关历史文档链接在GitHub上可能不可直接下载，CSV/JSON摘要保留，构建/测试结果可本地重新生成。

## 行结束与证据

.gitattributes使用-text保留源文件原始字节，避免Git自动换行转换造成源码SHA256审查基线失配。不要未经核对统一转换SDK和当前源码换行符。

## 推送

首个仓库创建为私有，当前账号zhangtao-0。上传前检查暂存区大小、构建引用、忽略规则和敏感信息；推送后核对远端提交与本地一致。仓库链接与提交信息记录在本目录上传记录中。
