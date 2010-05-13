#include <openssl/sha.h>
#include <fcntl.h>
#include <stdio.h>

char *get_sha1(char *filename)
{
	SHA_CTX context;

	unsigned char buf[SHA_DIGEST_LENGTH] = {0};
	static unsigned char ret[2*SHA_DIGEST_LENGTH + 1];
	int i;
	int fd;
	fd = open(filename, O_RDONLY);
	if (fd >= 0)
	{
		unsigned char buf2[1024];
		int len;
		SHA1_Init(&context);

		while ((len = read(fd, buf2, sizeof(buf2))) == sizeof(buf2))
			SHA1_Update(&context, buf2, sizeof(buf2));

		if (len)
			SHA1_Update(&context, buf2, len);

		SHA1_Final(buf, &context);
		close(fd);
	}

out:
	for (i = 0; i < SHA_DIGEST_LENGTH; i++)
		sprintf(&ret[2*i], "%2.2x", buf[i]);
	ret[2*SHA_DIGEST_LENGTH] = '\0';
	return ret;
}

#ifdef SHA1_TEST
int main(int argc, char *argv[])
{
	int i;
	for (i = 1; i < argc; i++)
		printf("%s\n", get_sha1(argv[i]));
	return 0;
}
#endif /* SHA1_TEST */
