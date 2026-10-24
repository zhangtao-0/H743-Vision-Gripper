#ifndef UI_CORE_H
#define UI_CORE_H
#include <stdint.h>
#define UI_DEPTH 6U
#define UI_ROWS 15U
#define UI_COLUMNS 30U
#define UI_LIST_ROWS 6U
typedef enum {UI_HOME,UI_PARAMS,UI_COMMANDS,UI_DEFAULTS,UI_MONITOR,UI_EDIT,UI_CONFIRM,UI_FACTORY,UI_HELP,UI_IMAGE,UI_VIEW,UI_EXEC_GROUP,UI_DEBUG} UiPage;
typedef enum {UI_NONE,UI_NEXT,UI_PREV,UI_OK,UI_BACK,UI_SHORTCUT} UiEvent;
typedef enum {UI_ACCEPTED,UI_OFFLINE,UI_BUSY,UI_BLOCKED,UI_STALE,UI_FAILED,UI_UNAVAILABLE} UiResult;
typedef enum {UI_TITLE_HOME,UI_TITLE_PARAMS,UI_TITLE_COMMANDS,UI_TITLE_DEFAULTS,UI_TITLE_MONITOR,UI_TITLE_EDIT,UI_TITLE_CONFIRM,UI_TITLE_FACTORY} UiTitle;
typedef struct {int32_t value,min,max,step,factory;} UiParameter;
typedef struct {
 uint8_t parameter_count,command_count,default_count;
 const char *(*parameter_name)(uint8_t);
 UiResult (*parameter_read)(uint8_t,UiParameter *);
 UiResult (*parameter_write)(uint8_t,int32_t,int32_t);
 const char *(*command_name)(uint8_t);
 UiResult (*command_run)(uint8_t);
 const char *(*default_name)(uint8_t);
 UiResult (*default_run)(uint8_t);
 void (*status_line)(uint8_t,uint8_t,char *,uint32_t);
 uint8_t (*command_group)(uint8_t);
 uint8_t help_count;
 const char *(*help_name)(uint8_t);
 UiResult (*help_print)(uint8_t);
} UiOperations;
typedef struct {
 UiPage page;uint8_t selected,top,tab,kind,item;
 int32_t draft,original,min,max,step;
} UiLocation;
typedef struct {
 UiLocation path[UI_DEPTH],saved[UI_DEPTH];uint8_t depth,saved_depth,has_saved;
 uint32_t revision;UiResult result;const UiOperations *ops;
} UiModel;
typedef struct {
 char row[UI_ROWS][UI_COLUMNS+1];uint8_t chinese[UI_ROWS],highlight[UI_ROWS],camera;
} UiView;
void UiCore_Init(UiModel *,const UiOperations *);
void UiCore_Event(UiModel *,UiEvent);
void UiCore_View(const UiModel *,UiView *);
const UiLocation *UiCore_Current(const UiModel *);
#endif
