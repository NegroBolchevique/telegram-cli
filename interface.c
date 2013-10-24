/*
    This file is part of telegram-client.

    Telegram-client is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 2 of the License, or
    (at your option) any later version.

    Telegram-client is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this telegram-client.  If not, see <http://www.gnu.org/licenses/>.

    Copyright Vitaly Valtman 2013
*/

#define _GNU_SOURCE 
#include <assert.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <readline/readline.h>
#include <readline/history.h>

#include "include.h"
#include "queries.h"

#include "interface.h"
#include "telegram.h"
#include "structures.h"

#include "mtproto-common.h"
char *default_prompt = "> ";

int unread_messages;

char *get_default_prompt (void) {
  static char buf[100];
  if (unread_messages) {
    sprintf (buf, COLOR_RED "[%d unread]" COLOR_NORMAL "%s", unread_messages, default_prompt);
    return buf;
  } else {
    return default_prompt;
  }
}

char *complete_none (const char *text UU, int state UU) {
  return 0;
}

char *commands[] = {
  "help",
  "msg",
  "contact_list",
  "stats",
  "history",
  "dialog_list",
  "send_photo",
  "send_video",
  "send_text",
  "chat_info",
  0 };

int commands_flags[] = {
  070,
  072,
  07,
  07,
  072,
  07,
  0732,
  0732,
  0732,
  074,
};

char *a = 0;
char *user_list[MAX_USER_NUM + 1];
char **chat_list = &a;

int init_token (char **q) {
  char *r = *q;
  while (*r == ' ') { r ++; }
  if (!*r) { return 0; }
  *q = r;
  return 1;
}

char *get_token (char **q, int *l) {
  char *r = *q;
  while (*r == ' ') { r ++; }
  if (!*r) { 
    *q = r; 
    *l = 0;
    return 0;
  }
  int neg = 0;
  char *s = r;
  while (*r && (*r != ' ' || neg)) {
    if (*r == '\\') {
      neg = 1 - neg;
    } else {
      neg = 0;
    }
    r++;
  }
  *q = r;
  *l = r - s;
  return s;
}


int get_complete_mode (void) {
  char *q = rl_line_buffer;
  if (!init_token (&q)) { return 0; }
  int l = 0;
  char *r = get_token (&q, &l);
  if (!*q) { return 0; }
  
  char **command = commands;
  int n = 0;
  int flags = -1;
  while (*command) {
    if (!strncmp (r, *command, l)) {
      flags = commands_flags[n];
      break;
    }
    n ++;
    command ++;
  }
  if (flags == -1) {
    return -1;
  }
  int s = 0;
  while (1) {
    get_token (&q, &l);
    if (!*q) { return flags ? flags & 7 : 7; }
    s ++;
    if (s <= 4) { flags >>= 3; }
  }
}

extern int user_num;
extern int chat_num;
extern union user_chat *Peers[];
int complete_user_list (int index, const char *text, int len, char **R) {
  index ++;
  while (index < user_num + chat_num && (!Peers[index]->print_name || strncmp (Peers[index]->print_name, text, len) || Peers[index]->id < 0)) {
    index ++;
  }
  if (index < user_num + chat_num) {
    *R = strdup (Peers[index]->print_name);
    return index;
  } else {
    return -1;
  }
}

int complete_chat_list (int index, const char *text, int len, char **R) {
  index ++;
  while (index < user_num + chat_num && (!Peers[index]->print_name || strncmp (Peers[index]->print_name, text, len) || Peers[index]->id > 0)) {
    index ++;
  }
  if (index < user_num + chat_num) {
    *R = strdup (Peers[index]->print_name);
    return index;
  } else {
    return -1;
  }
}

int complete_user_chat_list (int index, const char *text, int len, char **R) {
  index ++;
  while (index < user_num + chat_num && (!Peers[index]->print_name || strncmp (Peers[index]->print_name, text, len))) {
    index ++;
  }
  if (index < user_num + chat_num) {
    *R = strdup (Peers[index]->print_name);
    return index;
  } else {
    return -1;
  }
}

int complete_string_list (char **list, int index, const char *text, int len, char **R) {
  index ++;
  while (list[index] && strncmp (list[index], text, len)) {
    index ++;
  }
  if (list[index]) {
    *R = strdup (list[index]);
    return index;
  } else {
    *R = 0;
    return -1;
  }
}
char *command_generator (const char *text, int state) {  
  static int len, index, mode;
 
  if (!state) {
    len = strlen (text);
    index = -1;
     
    rl_line_buffer[rl_point] = '\0'; /* the effect should be such
      * that the cursor position
      * is at the end of line for
      * the auto completion regex
      * above (note the $ at end)
      */

    mode = get_complete_mode ();
  } else {
    if (index == -1) { return 0; }
  }

  if (mode == -1) { return 0; }

  char *R = 0;
  switch (mode & 7) {
  case 0:
    index = complete_string_list (commands, index, text, len, &R);
    return R;
  case 1:
    index = complete_user_list (index, text, len, &R);
    return R;
  case 2:
    index = complete_user_chat_list (index, text, len, &R);
    return R;
  case 3:
    return rl_filename_completion_function(text,state);
  case 4:
    index = complete_chat_list (index, text, len, &R);
    return R;
  default:
    return 0;
  }
}

char **complete_text (char *text, int start UU, int end UU) {
  return (char **) rl_completion_matches (text, command_generator);
}

void interpreter (char *line UU) {
  if (line && *line) {
    add_history (line);
  }
  if (!memcmp (line, "contact_list", 12)) {
    do_update_contact_list ();
  } else if (!memcmp (line, "dialog_list", 11)) {
    do_get_dialog_list ();
  } else if (!memcmp (line, "stats", 5)) {
    static char stat_buf[1 << 15];
    print_stat (stat_buf, (1 << 15) - 1);
    printf ("%s\n", stat_buf);
  } else if (!memcmp (line, "msg ", 4)) {
    char *q = line + 4;
    int len;
    char *text = get_token (&q, &len);
    int index = 0;
    while (index < user_num + chat_num && (!Peers[index]->print_name || strncmp (Peers[index]->print_name, text, len))) {
      index ++;
    }
    while (*q && (*q == ' ' || *q == '\t')) { q ++; }
    if (*q && index < user_num + chat_num) {
      do_send_message (Peers[index], q, strlen (q));
    }
  } else if (!memcmp (line, "send_photo", 10)) {
    char *q = line + 10;
    int len;
    char *text = get_token (&q, &len);
    int index = 0;
    while (index < user_num + chat_num && (!Peers[index]->print_name || strncmp (Peers[index]->print_name, text, len))) {
      index ++;
    }
    if (index < user_num + chat_num) {
      int len = 0;
      char *f = get_token (&q, &len);
      if (len > 0) {
        do_send_photo (CODE_input_media_uploaded_photo, 
        Peers[index]->id, strndup (f, len));
      }
    }
  } else if (!memcmp (line, "send_video", 10)) {
    char *q = line + 10;
    int len;
    char *text = get_token (&q, &len);
    int index = 0;
    while (index < user_num + chat_num && (!Peers[index]->print_name || strncmp (Peers[index]->print_name, text, len))) {
      index ++;
    }
    if (index < user_num + chat_num) {
      int len = 0;
      char *f = get_token (&q, &len);
      if (len > 0) {
        do_send_photo (CODE_input_media_uploaded_video, 
        Peers[index]->id, strndup (f, len));
      }
    }
  } else if (!memcmp (line, "send_text", 9)) {
    char *q = line + 10;
    int len;
    char *text = get_token (&q, &len);
    int index = 0;
    while (index < user_num + chat_num && (!Peers[index]->print_name || strncmp (Peers[index]->print_name, text, len))) {
      index ++;
    }
    if (index < user_num + chat_num) {
      int len = 0;
      char *f = get_token (&q, &len);
      if (len > 0) {
        do_send_text (Peers[index], strndup (f, len));
      }
    }
  } else if (!memcmp (line, "chat_info", 9)) {
    char *q = line + 10;
    int len;
    char *text = get_token (&q, &len);
    int index = 0;
    while (index < user_num + chat_num && (!Peers[index]->print_name || strncmp (Peers[index]->print_name, text, len))) {
      index ++;
    }
    if (index < user_num + chat_num && Peers[index]->id < 0) {
      do_get_chat_info (Peers[index]);
    }
  } else if (!memcmp (line, "history", 7)) {
    char *q = line + 7;
    int len;
    char *text = get_token (&q, &len);
    int index = 0;
    while (index < user_num + chat_num && (!Peers[index]->print_name || strncmp (Peers[index]->print_name, text, len))) {
      index ++;
    }
    if (index < user_num + chat_num) {
      char *text = get_token (&q, &len);
      int limit = 40;
      if (text) {
        limit = atoi (text);
        if (limit <= 0 || limit >= 1000000) {
          limit = 40;
        }
      }
      do_get_history (Peers[index], limit);
    }
  }
}

int readline_active;
void rprintf (const char *format, ...) {
  int saved_point = 0;
  char *saved_line = 0;
  if (readline_active) {
    saved_point = rl_point;
    saved_line = rl_copy_text(0, rl_end);
    rl_save_prompt();
    rl_replace_line("", 0);
    rl_redisplay();
  }

  va_list ap;
  va_start (ap, format);
  vfprintf (stdout, format, ap);
  va_end (ap);

  if (readline_active) {
    rl_restore_prompt();
    rl_replace_line(saved_line, 0);
    rl_point = saved_point;
    rl_redisplay();
    free(saved_line);
  }
}

int saved_point;
char *saved_line;
int prompt_was;
void print_start (void) {
  assert (!prompt_was);
  if (readline_active) {
    saved_point = rl_point;
    saved_line = rl_copy_text(0, rl_end);
    rl_save_prompt();
    rl_replace_line("", 0);
    rl_redisplay();
  }
  prompt_was = 1;
}
void print_end (void) {
  assert (prompt_was);
  if (readline_active) {
    rl_set_prompt (get_default_prompt ());
    rl_redisplay();
    rl_replace_line(saved_line, 0);
    rl_point = saved_point;
    rl_redisplay();
    free(saved_line);
  }
  prompt_was = 0;
}

void hexdump (int *in_ptr, int *in_end) {
  int saved_point = 0;
  char *saved_line = 0;
  if (readline_active) {
    saved_point = rl_point;
    saved_line = rl_copy_text(0, rl_end);
    rl_save_prompt();
    rl_replace_line("", 0);
    rl_redisplay();
  }
  int *ptr = in_ptr;
  while (ptr < in_end) { fprintf (stdout, " %08x", *(ptr ++)); }
  fprintf (stdout, "\n");
  
  if (readline_active) {
    rl_restore_prompt();
    rl_replace_line(saved_line, 0);
    rl_point = saved_point;
    rl_redisplay();
    free(saved_line);
  }
}

void logprintf (const char *format, ...) {
  int saved_point = 0;
  char *saved_line = 0;
  if (readline_active) {
    saved_point = rl_point;
    saved_line = rl_copy_text(0, rl_end);
    rl_save_prompt();
    rl_replace_line("", 0);
    rl_redisplay();
  }
  
  printf (COLOR_GREY " *** ");
  va_list ap;
  va_start (ap, format);
  vfprintf (stdout, format, ap);
  va_end (ap);
  printf (COLOR_NORMAL);

  if (readline_active) {
    rl_restore_prompt();
    rl_replace_line(saved_line, 0);
    rl_point = saved_point;
    rl_redisplay();
    free(saved_line);
  }
}

const char *message_media_type_str (struct message_media *M) {
  static char buf[1000];
  switch (M->type) {
    case CODE_message_media_empty:
      return "";
    case CODE_message_media_photo:
      return "[photo]";
    case CODE_message_media_video:
      return "[video]";
    case CODE_message_media_geo:
      sprintf (buf, "[geo] https://maps.google.com/maps?ll=%.6lf,%.6lf", M->geo.latitude, M->geo.longitude);
      return buf;
    case CODE_message_media_contact:
      snprintf (buf, 999, "[contact] " COLOR_RED "%s %s" COLOR_NORMAL " %s", M->first_name, M->last_name, M->phone);
      return buf;
    case CODE_message_media_unsupported:
      return "[unsupported]";
    default:
      assert (0);
      return "";

  }
}

int color_stack_pos;
const char *color_stack[10];

void push_color (const char *color) {
  assert (color_stack_pos < 10);
  color_stack[color_stack_pos ++] = color;
  printf ("%s", color);
}

void pop_color (void) {
  assert (color_stack_pos > 0);
  color_stack_pos --;
  if (color_stack_pos >= 1) {
    printf ("%s", color_stack[color_stack_pos - 1]);
  } else {
    printf ("%s", COLOR_NORMAL);
  }
}

void print_media (struct message_media *M) {
  switch (M->type) {
    case CODE_message_media_empty:
      return;
    case CODE_message_media_photo:
      if (M->photo.caption && strlen (M->photo.caption)) {
        printf ("[photo %s]", M->photo.caption);
      } else {
        printf ("[photo]");
      }
      return;
    case CODE_message_media_video:
      printf ("[video]");
      return;
    case CODE_message_media_geo:
      printf ("[geo] https://maps.google.com/?q=%.6lf,%.6lf", M->geo.latitude, M->geo.longitude);
      return;
    case CODE_message_media_contact:
      printf ("[contact] ");
      push_color (COLOR_RED);
      printf ("%s %s ", M->first_name, M->last_name);
      pop_color ();
      printf ("%s", M->phone);
      return;
    case CODE_message_media_unsupported:
      printf ("[unsupported]");
      return;
    default:
      assert (0);
  }
}

int unknown_user_list_pos;
int unknown_user_list[1000];

void print_user_name (int id, union user_chat *U) {
  push_color (COLOR_RED);
  if (!U) {
    printf ("user#%d", id);
    int i;
    for (i = 0; i < unknown_user_list_pos; i++) {
      if (unknown_user_list[i] == id) {
        id = 0;
        break;
      }
    }
    if (id) {
      assert (unknown_user_list_pos < 1000);
      unknown_user_list[unknown_user_list_pos ++] = id;
    }
  } else {
    if (U->flags & (FLAG_USER_SELF | FLAG_USER_CONTACT)) {
      push_color (COLOR_REDB);
    }
    if (!U->user.first_name) {
      printf ("%s", U->user.last_name);
    } else if (!U->user.last_name) {
      printf ("%s", U->user.first_name);
    } else {
      printf ("%s %s", U->user.first_name, U->user.last_name); 
    }
    if (U->flags & (FLAG_USER_SELF | FLAG_USER_CONTACT)) {
      pop_color ();
    }
  }
  pop_color ();
}

void print_chat_name (int id, union user_chat *C) {
  push_color (COLOR_MAGENTA);
  if (!C) {
    printf ("chat#%d", -id);
  } else {
    printf ("%s", C->chat.title);
  }
  pop_color ();
}

static char *monthes[] = {"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"};
void print_date (long t) {
  struct tm *tm = localtime (&t);
  if (time (0) - t < 12 * 60 * 60) {
    printf ("[%02d:%02d] ", tm->tm_hour, tm->tm_min);
  } else {
    printf ("[%02d %s]", tm->tm_mday, monthes[tm->tm_mon]);
  }
}

void print_date_full (long t) {
  struct tm *tm = localtime (&t);
  printf ("[%04d/%02d/%02d %02d:%02d:%02d]", tm->tm_year + 1900, tm->tm_mon + 1, tm->tm_mday, tm->tm_hour, tm->tm_min, tm->tm_sec);
}

int our_id;

void print_service_message (struct message *M) {
  print_start ();
  push_color (COLOR_GREY);

  print_date (M->date);
  printf (" ");
  print_chat_name (M->to_id, user_chat_get (M->to_id));
  printf (" ");
  print_user_name (M->from_id, user_chat_get (M->from_id));
 
  switch (M->action.type) {
  case CODE_message_action_empty:
    printf ("\n");
    break;
  case CODE_message_action_chat_create:
    printf (" created chat %s. %d users\n", M->action.title, M->action.user_num);
    break;
  case CODE_message_action_chat_edit_title:
    printf (" changed title to %s\n", 
      M->action.new_title);
    break;
  case CODE_message_action_chat_edit_photo:
    printf (" changed photo\n");
    break;
  case CODE_message_action_chat_delete_photo:
    printf (" deleted photo\n");
    break;
  case CODE_message_action_chat_add_user:
    printf (" added user ");
    print_user_name (M->action.user, user_chat_get (M->action.user));
    printf ("\n");
    break;
  case CODE_message_action_chat_delete_user:
    printf (" deleted user ");
    print_user_name (M->action.user, user_chat_get (M->action.user));
    printf ("\n");
    break;
  default:
    assert (0);
  }
  pop_color ();
  print_end ();
}

void print_message (struct message *M) {
  if (M->service) {
    print_service_message (M);
    return;
  }


  print_start ();
  if (M->to_id >= 0) {
    if (M->out) {
      push_color (COLOR_GREEN);
      print_date (M->date);
      pop_color ();
      printf (" ");
      print_user_name (M->to_id, user_chat_get (M->to_id));
      push_color (COLOR_GREEN);
      if (M->unread) {
        printf (" <<< ");
      } else {
        printf (" ««« ");
      }
    } else {
      push_color (COLOR_BLUE);
      print_date (M->date);
      pop_color ();
      printf (" ");
      print_user_name (M->from_id, user_chat_get (M->from_id));
      push_color (COLOR_BLUE);
      if (M->unread) {
        printf (" >>> ");
      } else {
        printf (" »»» ");
      }
    }
  } else {
    push_color (COLOR_MAGENTA);
    print_date (M->date);
    pop_color ();
    printf (" ");
    print_chat_name (M->to_id, user_chat_get (M->to_id));
    printf (" ");
    print_user_name (M->from_id, user_chat_get (M->from_id));
    if (M->from_id == our_id) {
      push_color (COLOR_GREEN);
    } else {
      push_color (COLOR_BLUE);
    }
    if (M->unread) {
      printf (" >>> ");
    } else {
      printf (" »»» ");
    }
  }
  if (M->message && strlen (M->message)) {
    printf ("%s", M->message);
  }
  if (M->media.type != CODE_message_media_empty) {
    print_media (&M->media);
  }
  pop_color ();
  assert (!color_stack_pos);
  printf ("\n");
  print_end();
}
