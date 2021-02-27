#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <malloc.h>
#include <errno.h>
#include <stdarg.h>
#include <unistd.h>

#include <fcntl.h>

#include <sys/types.h>

#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#include "jpeglib.h"

#include <3ds.h>

#define PORT 6543
#define PORT_INPUTS 6542

#define WIDTH 320
#define HEIGHT 240
#define DEPTH 24
#define PITCH (WIDTH * DEPTH/8)

#define SOC_ALIGN       0x1000
//#define SOC_BUFFERSIZE  0x100000
#define SOC_BUFFERSIZE  (1024 * 128)

static u32 *SOC_buffer = NULL;

__attribute__((format(printf,1,2)))
void failExit(const char *fmt, ...);


//---------------------------------------------------------------------------------
void socShutdown() {
//---------------------------------------------------------------------------------
	printf("waiting for socExit...\n");
	socExit();

}

int createServer(unsigned short port) {
  int server_fd;
  struct sockaddr_in address;
  int addrlen = sizeof(address);
  int opt;

  if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
    printf("ERR socket\n");
    return -1;
  }

  address.sin_family = AF_INET;
  address.sin_addr.s_addr = INADDR_ANY;
  address.sin_port = htons(port);

  if (bind(server_fd, (struct sockaddr *)&address, sizeof(address))<0) {
    printf("ERR bind\n");
    return -1;
  }

  if (listen(server_fd, 3) < 0) {
    printf("ERR listen\n");
    return -1;
  }

  int new_socket;
  if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen))<0) {
    printf("ERR accept\n");
    return -1;
  }
  fcntl(new_socket, F_SETFL, fcntl(new_socket, F_GETFL, 0) | O_NONBLOCK);

  shutdown(server_fd, SHUT_RDWR);
  closesocket(server_fd);

  return new_socket;
}

METHODDEF(int) jpegDecompress(struct jpeg_decompress_struct *cinfo,
                              char *outputBuf);


struct jpeg_decompress_struct cinfo;
struct jpeg_error_mgr jerr;
GLOBAL(int)
read_JPEG_buf(char *inputBuf, int inputSize, char *outputBuf)
{
  memset(&cinfo, 0, sizeof(cinfo));
  memset(&jerr, 0, sizeof(jerr));

  cinfo.err = jpeg_std_error(&jerr);
  jpeg_create_decompress(&cinfo);
  jpeg_mem_src(&cinfo, inputBuf, inputSize);

  int result = jpegDecompress(&cinfo, outputBuf);
  return result;
}

METHODDEF(int)
jpegDecompress(struct jpeg_decompress_struct *cinfo, char *outputBuf)
{
  (void)jpeg_read_header(cinfo, TRUE);
  cinfo->out_color_space = JCS_EXT_BGR;

  (void)jpeg_start_decompress(cinfo);
  int row_stride = cinfo->output_width * cinfo->output_components;

  char *rowsBuffer[1];
  rowsBuffer[0] = outputBuf;

  while (cinfo->output_scanline < cinfo->output_height) {
    (void)jpeg_read_scanlines(cinfo, rowsBuffer, 1);
    outputBuf += row_stride;
    rowsBuffer[0] = outputBuf;
  }

  (void)jpeg_finish_decompress(cinfo);
  jpeg_destroy_decompress(cinfo);

  return 1;
}

int readNBytes(int socket, char *buf, int n) {
    int total = 0;

    while(n > 0) {
      int rd = recv(socket, buf, (n > 1024) ? 1024 : n, 0);
      if ((rd == -1) && (errno == EAGAIN)) rd = 0;

      if (rd < 0) {
        return -1;
      }

      total += rd;
      n -= rd;
      buf += rd;
    }

    return total;
}


int readData(int socket, char *buf, unsigned short total, unsigned short read) {
    while(read < total) {
      int remain = total - read;
      int rd = recv(socket, buf+read, (remain > 1024) ? 1024 : remain, 0);
      if ((rd == -1) && (errno == EAGAIN)) rd = 0;

      if (rd < 0) {
        return -1;
      }

      if (rd == 0) break;
      read += rd;
    }

    return read;
}


u32 lastButtons = 0;
u16 updateKeys() {
        hidScanInput();

	lastButtons |= hidKeysDown();
	lastButtons ^= hidKeysUp();

	u16 data = 0;
	if (lastButtons & KEY_A) data |= 1;
	if (lastButtons & KEY_B) data |= 2;
	if (lastButtons & KEY_X) data |= 4;
	if (lastButtons & KEY_Y) data |= 8;
	if (lastButtons & KEY_DRIGHT) data |= 16;
	if (lastButtons & KEY_DLEFT) data |= 32;
	if (lastButtons & KEY_DUP) data |= 64;
	if (lastButtons & KEY_DDOWN) data |= 128;
	if (lastButtons & KEY_L) data |= 256;
	if (lastButtons & KEY_R) data |= 512;
	if (lastButtons & KEY_START) data |= 1024;
	if (lastButtons & KEY_SELECT) data |= 2048;

	return data;
}

touchPosition touch;
void sendInputs(int socket) {
	u16 btns = updateKeys();
	send(socket, &btns, 2, 0);

	circlePosition pos;
	hidCircleRead(&pos);
	send(socket, &pos, sizeof(pos), 0);

	hidTouchRead(&touch);
	send(socket, &touch, sizeof(touch), 0);
}


typedef struct {
  long long sz;
  int rd;
} HeaderBuf;
HeaderBuf header = {0, 0};

typedef struct {
  unsigned char *data;
  int rd;
} DataBuf;
DataBuf data = {0, 0};

char *outputBuf[WIDTH * HEIGHT * DEPTH/8];
//---------------------------------------------------------------------------------
int main(int argc, char **argv) {
//---------------------------------------------------------------------------------
	int ret;
	char *inputBuf = 0;
	int frame = 0;


	gfxInitDefault();

	// register gfxExit to be run when app quits
	// this can help simplify error handling
	atexit(gfxExit);

	consoleInit(GFX_TOP, NULL);

	printf ("\nCTroll3D\n");

	// allocate buffer for SOC service
	SOC_buffer = (u32*)memalign(SOC_ALIGN, SOC_BUFFERSIZE);

	if(SOC_buffer == NULL) {
		failExit("memalign: failed to allocate\n");
	}

	// Now intialise soc:u service
	if ((ret = socInit(SOC_buffer, SOC_BUFFERSIZE)) != 0) {
    	failExit("socInit: 0x%08X\n", (unsigned int)ret);
	}

	// register socShutdown to run at exit
	// atexit functions execute in reverse order so this runs before gfxExit
	atexit(socShutdown);

        gfxSetDoubleBuffering(GFX_BOTTOM, false);
        u8* fb = gfxGetFramebuffer(GFX_BOTTOM, GFX_LEFT, NULL, NULL);

        gspWaitForVBlank();
        hidScanInput();

	int sock = createServer(PORT);
	int socketInputs = createServer(PORT_INPUTS);

	int received = 0;

	int confirmationsDisabled = 0;
	while(aptMainLoop() && (socket > 0)) {
		if ((lastButtons & KEY_START) && (lastButtons & KEY_SELECT)) break;
		if ((lastButtons & KEY_L) && (lastButtons & KEY_R) && (lastButtons & KEY_DDOWN) && (touch.px > 0)) confirmationsDisabled = 1;
		if ((lastButtons & KEY_L) && (lastButtons & KEY_R) && (lastButtons & KEY_DUP) && (touch.px > 0) && confirmationsDisabled) {
			confirmationsDisabled = 0;
			received = 0;
			char b = 1;
			send(sock, &b, 1, 0);
                }

                gfxFlushBuffers();
                gfxSwapBuffers();

		if (header.rd < 8) {
			header.rd = readData(sock, (char *)&header.sz, 8, header.rd);
			if (header.rd < 8) {
				sendInputs(socketInputs);
				continue;
			}
			data.rd = 0;
			data.data = (char *) realloc(data.data, header.sz);
		}

		if (header.sz > 0) {
			if (data.rd < header.sz) {
				data.rd = readData(sock, (char *)data.data, header.sz, data.rd);
				if (data.rd < header.sz) {
					sendInputs(socketInputs);
					continue;
				}
			}

 			++received;
			if (received == 1) {
				if (!confirmationsDisabled) {
					char b = 1;
					send(sock, &b, 1, 0);
				}
			} else if (received == 2) {
				received = 0;
			}

			read_JPEG_buf(data.data, header.sz, outputBuf);
	                gspWaitForVBlank();

//			memcpy(fb, outputBuf, WIDTH * HEIGHT * DEPTH/8);
//The buffer is rotated on 3DS

			char *ptr = outputBuf;
			for (int y=0; y<HEIGHT; y++) {
				for (int x=0; x<WIDTH; x++) {
					int p = (x*HEIGHT*3) + (HEIGHT-y-1)*3; //(y*WIDTH*3) * x*3;
					fb[p + 0] = ptr[0];
					fb[p + 1] = ptr[1];
					fb[p + 2] = ptr[2];
					ptr += 3;
				}
			}
		}

		header.rd = 0;
		sendInputs(socketInputs);

	}
	close(socket);


	return 0;
}

//---------------------------------------------------------------------------------
void failExit(const char *fmt, ...) {
//---------------------------------------------------------------------------------
	va_list ap;

	printf(CONSOLE_RED);
	va_start(ap, fmt);
	vprintf(fmt, ap);
	va_end(ap);
	printf(CONSOLE_RESET);
	printf("\nPress B to exit\n");

	while (aptMainLoop()) {
		gspWaitForVBlank();
		hidScanInput();

		u32 kDown = hidKeysDown();
		if (kDown & KEY_B) exit(0);
	}
}
