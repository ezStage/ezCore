#ifndef _EZ_ML_H
#define _EZ_ML_H

extern ezDict *ezML_load_file(ezDict *dict, const char *file);
extern ezDict *ezML_load_string(ezDict *dict, const char *buf, int len);

extern void ezML_store_file(ezDict *dict, const char *file);

#endif

