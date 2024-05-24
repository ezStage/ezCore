#include <ezCore/ezCore.h>

int main(int argc, char *argv[])
{
	char *p = "  hello world  ";

	ezString * s1 = ezString_create(p, -1);
	ezString * s2 = ezString_create_trim(p, -1);
	ezString * s3 = ezString_printf("ezString_printf(\"%s\")", p);
	printf("s1[%d]: \"%s\"\n", ezStrLen(s1), ezStr(s1));
	printf("s2[%d]: \"%s\"\n", ezStrLen(s2), ezStr(s2));
	printf("s3[%d]: \"%s\"\n", ezStrLen(s3), ezStr(s3));

	char *p1 = "123";
	s3 = ezString_append(s2, p1);
	printf("\"%s\" + \"%s\" -> \"%s\"\n", ezStr(s2), p1, ezStr(s3));

	s3 = ezString_prepend(s2, p1);
	printf("\"%s\" + \"%s\" -> \"%s\"\n", p1, ezStr(s2), ezStr(s3));

	int in = ezString_index(s2, 0, 'w');
	s3 = ezString_delete(s2, in, 5);
	printf("\"%s\" - \"world\" -> \"%s\"\n", ezStr(s2), ezStr(s3));
	s3 = ezString_delete(s2, in, 3);
	printf("\"%s\" - \"wor\" -> \"%s\"\n", ezStr(s2), ezStr(s3));

	s3 = ezString_insert(s2, in, p1);
	printf("\"%s\" insert \"%s\" at w -> \"%s\"\n", ezStr(s2), p1, ezStr(s3));

	s3 = ezString_replace(s2, in, 5, p1);
	printf("\"%s\" replace \"world\" with \"%s\" -> \"%s\"\n", ezStr(s2), p1, ezStr(s3));
	s3 = ezString_replace(s2, in, 3, p1);
	printf("\"%s\" replace \"wor\" with \"%s\" -> \"%s\"\n", ezStr(s2), p1, ezStr(s3));

	char *p2 = " s1; s2 ;s3 ;d4 ; d - 5; ";
	ezVector *rv = ezString_separate(p2, -1, ';');
	printf("ezString_separate(\"%s\")\n ->", p2);
	ezStringVector_dump(rv);

	ezGC_exit(0);
	return 0;
}

