#include <stdint.h>
typedef struct {uint8_t moving;} FeetechStsFeedback_t;
const FeetechStsFeedback_t *FeetechStsBus_GetFeedback(void);
