#include <stdio.h>
#include <stdlib.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <unistd.h>

#include <string.h>

#include <stdint.h>

#define MAGIC_CODE 1146110283

uint32_t endian_32 (uint32_t num) {
	unsigned char a[4];
	unsigned char b[4];

	memcpy (a, &num, sizeof (a));

	b[0] = a[3];
	b[1] = a[2];
	b[2] = a[1];
	b[3] = a[0];

	memcpy (&num, b, sizeof (a));

	return num;
}

uint16_t endian_16 (uint16_t num) {
	unsigned char a[2];
	unsigned char b[2];

	memcpy (a, &num, sizeof (a));

	b[0] = a[1];
	b[1] = a[0];

	memcpy (&num, b, sizeof (b));

	return num;
}

void get_basename (char *file, char *basename) {
	int g;

	g = 0;
	while (file[g] != 0 && file[g] != '.') {
		basename[g] = file[g];
		g++;
	}
	basename[g] = 0;
}

int main (int argc, char *argv[]) {
	int fd, subfd;

	uint32_t temp;
	uint16_t t16;
	int archivos;
	int g;
	char **nombre;
	char buffer[2048];
	char basename[1024];
	char dest_file[2048];
	int r, t, a;
	ssize_t bytes_read;
	size_t nbytes, wbytes;

	int *lens;
	if (argc < 2) {
		printf ("Uso: %s [archivo]\n", argv[0]);

		return 0;
	}

	get_basename (argv[1], basename);

#ifdef _WIN32
	if (mkdir (basename) < 0) {
#else
	if (mkdir (basename, 0755) < 0) {
#endif
		perror ("Error al crear el directorio");
		//return EXIT_FAILURE;
	}

	fd = open (argv[1], _O_BINARY | _O_RDONLY);

	if (fd < 0) {
		printf ("Falló al abrir el archivo \"%s\"\n", argv[1]);
		return EXIT_FAILURE;
	}

	read (fd, &temp, sizeof (temp));
	//temp = endian_32 (temp);

	if (temp != MAGIC_CODE) {
		printf ("Magic code failed\n");

		close (fd);
		return EXIT_FAILURE;
	}

	read (fd, &t16, sizeof (t16));
	//t16 = endian_16 (t16);

	printf ("Número de archivos en el DPACK: %i\n", t16);

	archivos = t16;

	lens = (int *) malloc (archivos * sizeof (int));
	nombre = (char **) malloc (archivos * sizeof (char *));

	for (g = 0; g < archivos; g++) {
		read (fd, &temp, sizeof (temp));
		//temp = endian_32 (temp);
		lens[g] = temp;

		printf ("Longitud del archivo %i = %i bytes (creo)\n", g + 1, lens[g]);
	}

	for (g = 0; g < archivos; g++) {
		read (fd, &t16, sizeof (t16));

		nombre[g] = (char *) malloc (sizeof (char) * (t16 + 1));

		read (fd, nombre[g], t16);
		nombre[g][t16] = 0;
		printf ("Nombre del archivo %i = %s\n", g + 1, nombre[g]);
	}

	printf ("Leyendo archivos...\n");
	off_t post;
	for (g = 0; g < archivos; g++) {
		sprintf (dest_file, "%s/%s", basename, nombre[g]);
		subfd = open (dest_file, _O_BINARY | O_WRONLY | O_CREAT | O_TRUNC, 0640);

		if (subfd < 0) {
			perror ("Crear");
			printf ("No se pudo crear el archivo %s para su escritura\n", dest_file);
			continue;
		}

		printf ("Leyendo el archivo %s. Total = %i\n", nombre[g], lens[g]);
		r = lens[g];

		while (r > 0) {
			if (r < 1024) {
				nbytes = r;
			} else {
				nbytes = 1024;
			}
			bytes_read = read (fd, buffer, nbytes);

			printf ("%i bytes leidos, restan %i\n", bytes_read, r);
			/*if (bytes_read <= 0) {
				printf ("Deteniendo\n");
				exit (2);
			}*/
			wbytes = nbytes;
			write (subfd, buffer, wbytes);
			r = r - nbytes;
		}

		close (subfd);
	}

	close (fd);
	return 0;
}
