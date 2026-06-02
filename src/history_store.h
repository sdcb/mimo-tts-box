#pragma once

#include "app_types.h"

BOOL history_save_pending(const RequestTask *task);
BOOL history_save_result(const RequestResult *result);
BOOL history_load_recent(HistoryRecord **records, int *count);
BOOL history_load_detail(const wchar_t *dir_name, char **request_json, char **response_json, HistoryRecord *metadata);
void history_free_records(HistoryRecord *records, int count);
