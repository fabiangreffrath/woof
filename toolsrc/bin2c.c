#include <stdio.h>
#include <string.h>
#include <stdlib.h>

int main(int argc,char **argv)
{
  char buf[256],s[100];
  char *name,*ext;
  int c;
  FILE *fp;
  if (argc!=2)
    {
      fprintf(stderr,"Usage: %s file\n",*argv);
      return 1;
    }
  if (!(fp = fopen(argv[1],"rb")))
    {
      fputs("Cannot open ",stderr);
      perror(argv[1]);
      return 1;
    }
  name = strdup(argv[1]);
  if ((ext = strrchr(name, '.')))
    *ext = '\0';
  printf("static unsigned char %s[] = {\n", name);
  free(name);
  strcpy(buf,"  ");
  while ((c=getc(fp))!=EOF)
    {
      sprintf(s,"%u,",(unsigned char) c);
      if (strlen(s)+strlen(buf) >= 80)
	puts(buf), strcpy(buf,"  ");
      strcat(buf,s);
    }
  if (*buf)
    strcat(buf,"\n");
  printf("%s};\n",buf);
  fclose(fp);
  return 0;
}
