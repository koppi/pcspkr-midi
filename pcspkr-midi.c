/*
 * pcspkr-midi.c - Turn your pc speaker into an ALSA MIDI device.
 *
 * Copyright (C) 2013-2022 Jakob Flierl <jakob.flierl@gmail.com>
 *
 *  This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Lesser General Public
 *  License as published by the Free Software Foundation:
 *  version 2.1 of the License.
 *
 *  This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public
 *  License along with this library; if not, write to the Free Software
 *  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 *  MA  02110-1301  USA
 *
 * ## Usage:
 *
 * $ cat /usr/lib/udev/rules.d/70-pcspkr-beep.rules
   ACTION=="add", SUBSYSTEM=="input", ATTRS{name}=="PC Speaker", ENV{DEVNAME}!="", GROUP="audio", MODE="0620"
   $ sudo rmmod pcspkr
   $ sudo modprobe pcspkr
   $ sudo udevadm control --reload
   $ make LDLIBS="-lasound -lm" pcspkr-midi
   $ ./pcspkr-midi
   $ aconnect -o
   $ aconnect -i
   $ aconnect <input> <output>
   $ # Next, play some notes on your MIDI keybard.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include <linux/input.h>
#include <linux/kd.h>
#include <sys/ioctl.h>

#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <math.h>
#include <time.h>
#include <errno.h>
#include <signal.h>
#include <poll.h>

#include <alsa/asoundlib.h>

#define ARRAY_SIZE(a) (sizeof(a) / sizeof *(a))

static snd_seq_t *seq;
static volatile sig_atomic_t signal_received = 0;

/* prints an error message to stderr, and dies */
static void fatal(const char *msg, ...)
{
        va_list ap;

        va_start(ap, msg);
        vfprintf(stderr, msg, ap);
        va_end(ap);
        fputc('\n', stderr);
        exit(EXIT_FAILURE);
}

/* memory allocation error handling */
static void check_mem(void *p)
{
 	if (!p)
                fatal("out of memory");
}

/* error handling for ALSA functions */
static void check_snd(const char *operation, int err)
{
 	if (err < 0)
                fatal("cannot %s - %s", operation, snd_strerror(err));
}

static void sighandler(int sig)
{
        signal_received = 1;
}

snd_seq_addr_t input_addr;

static void wait_ms(double t) {
  struct timespec ts;

  ts.tv_sec = t / 1000;
  ts.tv_nsec = (t - ts.tv_sec * 1000) * 1000000;
  nanosleep(&ts, NULL);
}

void beep(int fd, int frq) {
  struct input_event ev;
  ev.type = EV_SND;
  ev.code = SND_TONE;
  ev.value = frq;

  int result;

  result = write(fd, (const void *)&ev, sizeof(ev));
  if (result < 0) {
    fprintf(stderr, "error writing in beep=%s\n", strerror(errno));
  }
}

int main(int argc, char** argv) {
  int err;
  
  err = snd_seq_open(&seq, "default", SND_SEQ_OPEN_DUPLEX, 0);
  check_snd("open sequencer", err);

  err = snd_seq_set_client_name(seq, "pcspkr-midi");
  check_snd("set client name", err);
  int client = snd_seq_client_id(seq);
  check_snd("get client id", client);

  int port = snd_seq_create_simple_port(seq, "pcspkr-midi",
					SND_SEQ_PORT_CAP_WRITE | SND_SEQ_PORT_CAP_SUBS_WRITE | SND_SEQ_PORT_CAP_SYNC_WRITE,
					SND_SEQ_PORT_TYPE_APPLICATION);
  check_snd("create port", port);

  printf("Opened ALSA Midi client:port %d:%d\n", client, port);
  
  int fd = open("/dev/input/by-path/platform-pcspkr-event-spkr", O_WRONLY, 0);

  if (fd == -1) {
    fprintf(stderr, "%d: could not open speaker device. Did you \"sudo modprobe pcspkr\"?\n", fd);
  }

  char namebuf[128];

  if (ioctl(fd, EVIOCGNAME(sizeof(namebuf)), namebuf) < 0) {
    return 1;
  }
  
  struct input_id inpid;
  if (ioctl(fd, EVIOCGID, &inpid) < 0) {
    return 1;
  }

  fprintf(stderr, "Found \"%s\": bustype = %d, vendor = 0x%.4x, product = 0x%.4x, version = %d\n",
	  namebuf, inpid.bustype, inpid.vendor, inpid.product, inpid.version);

  signal(SIGINT,  sighandler);
  signal(SIGTERM, sighandler);

#define c 262*2

  int pollfds_count = snd_seq_poll_descriptors_count(seq, POLLIN);
  struct pollfd *pollfds = alloca(pollfds_count * sizeof *pollfds);
  err = snd_seq_poll_descriptors(seq, pollfds, pollfds_count, POLLIN);
  check_snd("get poll descriptors", err);
  pollfds_count = err;

  while(1) {
    snd_seq_event_t *rec_ev;
    rec_ev = NULL;
  retry:
    err = poll(pollfds, pollfds_count, 1000);
    if (signal_received)
      break;
    if (err == 0)
      goto retry; // timeout
    if (err < 0)
      fatal("poll error: %s", strerror(errno));
    unsigned short revents;
    err = snd_seq_poll_descriptors_revents(seq, pollfds, pollfds_count, &revents);
    check_snd("get poll events", err);
    if (revents & (POLLERR | POLLNVAL))
      break;
    if (!(revents & POLLIN))
      continue;
    err = snd_seq_event_input(seq, &rec_ev);
    check_snd("input MIDI event", err);
    
    if (rec_ev->type == SND_SEQ_EVENT_NOTEON) {
      printf("NOTE on %d\n", rec_ev->data.note.note);
      double f = 440.0*exp(((rec_ev->data.note.note-69.0)/12.0)*log(2.0));
      beep(fd, f);
    } else if (rec_ev->type == SND_SEQ_EVENT_NOTEOFF) {
      printf("NOTE off\n");
      beep(fd, 0);
    } else if (rec_ev->type == SND_SEQ_EVENT_PORT_SUBSCRIBED) {
      printf("SND_SEQ_EVENT_PORT_SUBSCRIBED\n");
    } else if (rec_ev->type == SND_SEQ_EVENT_PORT_UNSUBSCRIBED) {
      printf("SND_SEQ_EVENT_PORT_UNSUBSCRIBED\n");
      beep(fd, 0);
    }
    
    snd_seq_free_event(rec_ev);
  }
  
  beep(fd, 0);

  close(fd);

  return 0;
}
