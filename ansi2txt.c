/*
 *     ansi2txt - A simple program that makes vt100/ansi terminal streams
 *                readable on text or html displays
 * 
 *     Copyright (C) 2007 Emmet Spier
 * 
 *     This program is free software; you can redistribute it and/or modify
 *     it under the terms of the GNU General Public License as published by
 *     the Free Software Foundation; either version 2 of the License, or
 *     (at your option) any later version.
 * 
 *     This program is distributed in the hope that it will be useful,
 *     but WITHOUT ANY WARRANTY; without even the implied warranty of
 *     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *     GNU General Public License for more details.
 * 
 *     You should have received a copy of the GNU General Public License
 *     along with this program; if not, write to the Free Software
 *     Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define VERSION "0.2.2"

#define DEFAULT_HEIGHT 60
#define DEFAULT_WIDTH  120

#define VT100_BUF 8
#define VT100_PARAMS 10
#define TAB 9
char *progname;

#define mode_bright       1
#define mode_dim          2
#define mode_underscore   8
#define mode_blink        16
#define mode_reverse      64
#define mode_hidden       128

// this extra one to support graphics characters (a bit)
#define mode_graphics     32

#define mode_wide         1
#define mode_tall_top     2
#define mode_tall_bot     4
  
#define m_black           0
#define m_red             1
#define m_green           2
#define m_yellow          3
#define m_blue            4
#define m_magenta         5
#define m_cyan            6
#define m_white           7

#define m_black_n_white  m_black+16*m_white
#define m_white_n_black  m_white+16*m_black

int use_stdin     = 0;
int html_mode     = -1;
int html_refresh  = 0;
int reverse_video = 0;

int  cX = 0, cY = 0;
int  max_cX = 0, max_cY = 0;
char *out = 0, *out_mode = 0, *out_col = 0, *out_size = 0;
int  width  = DEFAULT_WIDTH;
int  height = DEFAULT_HEIGHT;

char *colour_names[] = {"#000000", "#bb0000", "#00bb00", "#bbbb00", "#0000bb", "#bb00bb", "#00bbbb", "#cccccc", // normal
                        "#555555", "#ff0000", "#00ff00", "#ffff00", "#0000ff", "#ff00ff", "#00ffff", "#ffffff", // bright fg
                        "#000000", "#660000", "#006600", "#666600", "#000066", "#660066", "#006666", "#999999", // dim fg
                        };

char *graphics_chars[] = {" ", "&loz;", "&equiv;", "*", "*", "*", "*", "&deg;",
                          "&plusmn;", "*", "*", "&rfloor;", "&rceil;", "&lceil;", "&lfloor;", "+",
  "&macr;", "<span style=\"position:relative;bottom:-0.17em\">&macr;</span>", "&mdash;", "<span style=\"position:relative;bottom:+0.17em\">_</span>", "_", "<span style=\"margin: -0.1ex; letter-spacing:-0.6ex\">|-</span>", "<span style=\"margin: -0.1ex; letter-spacing:-0.6ex\">-|</span>", "&perp;",
  	                  "T", "|", "&le;", "&ge;", "&pi;", "&ne;", "&pound;", "&middot;"
                         };
  
                         
                         
void usage(void) {
printf("%s: [-w WIDTH] [-h HEIGHT] [-rv] [-html|-txt] [-refresh secs] [INPUT]\n", progname);
printf("%s: -v       version\n", progname);
printf("%s: --help   help\n", progname);
printf("\n");
}

void clear_cells(int start, int len){//(char* b1, char* b2, char* b3, int start, int len){
//  memset(b1+start, 32, len);                       // ' '
//  memset(b2+start, 0,  len);                       // mode = 0
//  memset(b3+start, m_white + m_black >> 4, len);   // fg white, bg black 
  memset(out     + start, 32, len);                       // ' '
  memset(out_mode+ start, 0,  len);                       // mode = 0
  memset(out_col + start, m_white + m_black >> 4, len);   // fg white, bg black 
}

void print_line(int line);

int main(int argc, char **argv)
{
  int loop, tmp;

  progname = argv[0];

  ++argv; --argc;
  while ((argc >= 1) && (**argv == '-')) {
    tmp = strlen(*argv);
    if (strncmp(*argv, "--help", tmp) == 0) {
      usage();
      return 0;
    }
    else if (strncmp(*argv, "-v", tmp) == 0) {
      printf("ansi2txt - version %s, compiled on %s at %s.\n", VERSION, __DATE__, __TIME__);
      return 0;
    }
    else if (strncmp(*argv, "-rv", tmp) == 0) {   
      reverse_video = 1;
    }
    else if (strncmp(*argv, "-h", tmp) == 0) {
      if (argc > 1) {
        height = atoi(argv[1]);
        ++argv; --argc;
      }
      else {
        printf("\nMissing height from %s\n\n",argv[0]);
        usage(); return 0;
      }
    }
    else if (strncmp(*argv, "-w", tmp) == 0) {   
      if (argc > 1) {
        width  = atoi(argv[1]);
        ++argv; --argc;
      }
      else {
        printf("\nMissing width  from %s\n\n",argv[0]); 
        usage(); return 0;
      }
    }
    else if (strncmp(*argv, "-refresh", tmp) == 0) {   
      if (argc > 1) {
        html_refresh  = atoi(argv[1]);
        if (html_refresh < 0) html_refresh = 0;
        ++argv; --argc;
      }
      else {
        printf("\nMissing refresh time (in seconds) from %s\n\n",argv[0]); 
        usage(); return 0;
      }
    }
    else if (strncmp(*argv, "-html", tmp) == 0) {
      html_mode = 1;
    }
    else if (strncmp(*argv, "-txt", tmp) == 0) {
      if (html_mode!=-1) { printf("\nOptions -html and -txt must not be used together.\n\n",argv[0]); usage(); return 0;}
      else html_mode = 0;
    }
    else {
    printf("\nUnknown option %s\n\n",argv[0]); usage(); return 0;
    }

  ++argv; --argc;

  }

  if (html_mode == -1)
    if (strcasestr(progname, "html") != 0) html_mode = 1; else html_mode = 0;
  if (height < 1) height = DEFAULT_HEIGHT;
  if (width  < 1) width  = DEFAULT_WIDTH;
  if (argc < 1) use_stdin = 1;

  
  out      = (char*) malloc(width*height);
  out_mode = (char*) malloc(width*height);
  out_col  = (char*) malloc(width*height);
  out_size = (char*) malloc(height);
  
  //cls
  clear_cells(0, width*height);//out, out_mode, out_col, 0, width*height);
  memset(out_size, 0, height);
  
  if ((out==0)||(out_mode==0)||(out_col==0)||(out_size==0)) {
    printf("Memory allocation failure.\n");
    return 0;
  }
  
  
  char vt100[VT100_BUF];
  memset(vt100, 0, VT100_BUF);
  int  vt100_ptr = 0;
  int vt100_params[VT100_PARAMS];
  int param_ptr = 0;
  
  char current_mode = 0;
  char current_col  = m_white_n_black;
  int cX_save = 0, cY_save = 0;
  int q_mark = 0;
  char b = 0;

  FILE *f;
  if (use_stdin) f = stdin;
  else {
    f = fopen(argv[0], "r");
    if (f==NULL) { printf("File %s not found.\n", argv[0]); return 0; }
    }

  if (html_mode) {
    printf("<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 4.01 Transitional//EN\">\n"
           "<html><head>\n"
           "<meta http-equiv=\"content-type\" content=\"text/html; charset=ISO-8859-1\">\n");
    if (html_refresh) printf("<meta http-equiv=\"refresh\" content=\"%d\">\n", html_refresh);
    printf("<title></title><style type=\"text/css\">\n"
           "b {font-family: monospace; font-weight: normal;}"
           "</style></head><body");
    if (reverse_video) printf(" style=\"colour: %s; background: %s\">\n<pre><b>", colour_names[0], colour_names[7]);
    else               printf(" style=\"colour: %s; background: %s\">\n<pre><b>", colour_names[7], colour_names[0]);
  }

  while (!feof(f)) {
    b = fgetc(f);
    if ((b > 31) && (b < 127)) {
      tmp = cX + cY*width;
      out[tmp] = b;
      out_mode[tmp] = current_mode;
      out_col [tmp] = current_col;
      cX++;
    }
    
    else
 
    switch (b) { // not display char switch
    case 8: cX--; break;
    case 9: cX = (cX / TAB) + TAB; break;
    case 10: cX = 0;
    case 11: cY++; break;
    case 12: // ^L form feed
      for (cY = 0; cY <= max_cY; cY++) print_line(cY);
      clear_cells(0, width*height);//out, out_mode, out_col, 0, width*height);
      memset(out_size, 0, height);
      cX = 0; cY = 0; max_cX = 0; max_cY = 0;
      break;
    case 13: cX = 0; break;


    case 27: // ESC
      b = fgetc(f);

      switch (b) { // ESC switch

      case '7' : cX_save = cX; cY_save = cY; break; // cursor save
      case '8' : cX = cX_save; cY = cY_save; break; // cursor restore

      case '#' :
        b = fgetc(f);
        switch (b) {
        case '3' : out_size[cY] = mode_tall_top; break; // Double Height top line 
        case '4' : out_size[cY] = mode_tall_bot; break; // Double Height bottom line
        case '5' : out_size[cY] = 0;             break; // Single width line
        case '6' : out_size[cY] = mode_wide;     break; // Double width line
        default : ; // slurp
        }
        break;
      case 'P' : // Device Control String, we slurp it up until ESC \
        while (((b = fgetc(f)) != 27) && !feof(f)) ;
        b = fgetc(f);
        break;
      case '\\' : break; // Termimation code for a Device Control String

      case '(' : // Choose character set, we ignore        
        b = fgetc(f);
        switch (b) {
        case '0' : current_mode |= mode_graphics; break;
        default  : current_mode &= (0xff-mode_graphics); break;
        }
        break;

      case '[' :
        vt100_ptr = 0;
        for (loop = 0; loop < param_ptr; loop++) vt100_params[loop] = 0; // unset previous
        param_ptr = 0;

        b = ';'; q_mark = 0;
        while ( ( (b == ';') && (vt100_ptr < VT100_BUF)) && !feof(f)) {
          while ( ((b = fgetc(f)) <= '9') && !feof(f))  {
            vt100[vt100_ptr] = b;
            vt100_ptr++;
          }
          if (b == '?') {
            q_mark = 1;
            b = ';';
          }
          else {
            vt100[vt100_ptr] = 0;
            vt100_params[param_ptr++] = atoi(vt100);
            vt100_ptr = 0;
          }
        }
        if (q_mark == 1) {
          switch(b) { // q_mark switch
          case 8: cX--; break;
          case 9: cX = (cX / TAB) + TAB; break;
          case 10: cX = 0;
          case 11:
          case 12: cY++; break;
          case 13: cX = 0; break;
          default:;
          }
        }
        else 
          {
            switch (b) { // ESC action switch
            case 'H' : // tab (row, col)
            case 'f' :
              cY = (vt100_params[0] == 0) ? 0 : vt100_params[0] - 1;
              cX = (vt100_params[1] == 0) ? 0 :  vt100_params[1] - 1;
              break;
            case 'A': cY -= (vt100_params[0] > 0) ? vt100_params[0] : 1; break; // cursor up
            case 'B': cY += (vt100_params[0] > 0) ? vt100_params[0] : 1; break; // cursor down
            case 'C': cX += (vt100_params[0] > 0) ? vt100_params[0] : 1; break; // cursor right
            case 'D': cX -= (vt100_params[0] > 0) ? vt100_params[0] : 1; break; // cursor left
            case 'd': cY = (vt100_params[0] == 0) ? 0 : vt100_params[0] - 1; break; // vertical postion absolute
            case 'e': cY += vt100_params[0]; break;                                 // vertical postion relative
            case 's': cX_save = cX; cY_save = cY; break; // cursor save
            case 'u': cX = cX_save; cY = cY_save; break; // cursor restore
            case 'J' : // erase screen (from cursor)
              switch (vt100_params[0]) {
              case 1: clear_cells(0, cX - cY*width); break;
              case 2: clear_cells(0, width*height);  break;
              default:
                clear_cells(cX + cY*width, width*height - (cX + cY*width));
              }
              break;
            case 'K' : // erase line (from cursor)
              switch (vt100_params[0]) {
              case 1:  clear_cells(cY*width, cX);    break;
              case 2:  clear_cells(cY*width, width); break;
              default: clear_cells(cY*width + cX, width-cX);
              }
              break;
           case 'm' : // color info
              for (loop = 0; loop < param_ptr; loop++) {
                if (vt100_params[loop] <= 8)
                  if (vt100_params[loop] == 0)
                    current_mode &= mode_graphics; // reset all (graphics is not a mode)
                  else
                    current_mode |= 1<<(vt100_params[loop]-1);

                if ((vt100_params[loop] >= 30) && (vt100_params[loop] <= 37))
                  current_col = vt100_params[loop]-30 | (current_col&0xf0);
                if ((vt100_params[loop] >= 40) && (vt100_params[loop] <= 47))
                  current_col = ((vt100_params[loop]-40)*16) | (current_col&0xf);
              }
              break;
            case 'r' : // DEC termial top and bottom margin scroll free areas 
            case 'h' : // Mode Set (4 = insert; 20 = auto linefeed)
            case 'l' : // Mode Reset  (4 = insert; 20 = auto linefeed)
            case '?' : // more stuff to ignore...
              break; 
            default: ;
//              printf("<");        
//              for (loop = 0; loop < param_ptr; loop++) printf(" %d", vt100_params[loop]); 
//              printf(" '%c'>\n", b);     
            }
            break;
          default: ungetc(b, f);
//              printf("<< '%c' >>\n", b);     
          } // else
      }
    
    break;
    default:;
    }
    
    if (cX < 0)      {cX = width-1; cY--;}
    if (cY < 0)      {cY = 0;}
    if (cX >= width) {cX = 0; cY++;}
    
    if (cY >= height) { // height overflow so scroll buffer and print overflow
      tmp = cY-height+1;
      if (tmp >= height) tmp = height - 1;
      for (loop = 0; loop < tmp; loop++) print_line(loop);
      memmove(out     , out      + tmp*width, (height-tmp)*width);
      memmove(out_mode, out_mode + tmp*width, (height-tmp)*width);
      memmove(out_col , out_col  + tmp*width, (height-tmp)*width);
      memmove(out_size, out_size + tmp      ,  height-tmp       );
      clear_cells( (height-tmp)*width, tmp*width);
      
      cY = height - 1;
    }
    
    if (cX * ((out_size[cY]!=0)?2:1) > max_cX) max_cX = cX * ((out_size[cY]!=0)?2:1);
    if (cY > max_cY) max_cY = cY;
    }
  
  for (cY = 0; cY <= max_cY; cY++) print_line(cY);
  
  if (html_mode) printf("</b></pre></body></html>\n");

  if (!use_stdin) fclose(f);
  return 1;
}



void print_line(int line) {
  int print_col = 0, col_set = 1;;
  int wide_set = 0, tall_set = 0;
  int current_mode = -1;
  int current_col  = -1;
  int cX, cY = line;
  
  int tmp = cY*width;

    if (out_size[cY]&mode_wide) wide_set = 1; else wide_set = 0;
    if (out_size[cY]&mode_tall_top) tall_set = 1; else tall_set = 0;     
    if (out_size[cY]&mode_tall_bot) {
      if (html_mode) return; // html can't do halves so we just don't draw the top(!)
    }

    if (tall_set && html_mode) printf("</b><span style=\"font-size:190%;\"><b>");

    for (cX = 0; cX <= max_cX; cX++) {
      tmp = cX + cY*width;

      if (html_mode)
        if ((current_mode != out_mode[tmp]) || (current_col != out_col[tmp])) {

          print_col = out_col[tmp];

          if (out_mode[tmp]&mode_reverse)
            if (print_col&(m_black*16) == m_black_n_white) print_col = m_white_n_black;          // reverse b&w 
            else if (print_col == m_white_n_black) print_col = m_black_n_white;                  // reverse w&b
            else if ((print_col&0xf0) == (m_black*16)) print_col = (print_col&0xf) + m_white*16; // bg blk -> bg wht
            else if ((print_col&0xf0) == (m_white*16)) print_col = (print_col&0xf) + m_black*16; // bg wht -> bg blk
          if (reverse_video)
            if (print_col&(m_black*16) == m_black_n_white) print_col = m_white_n_black;          // reverse b&w
            else if (print_col == m_white_n_black) print_col = m_black_n_white;                  // reverse w&b
            else if ((print_col&0xf0) == (m_black*16)) print_col = (print_col&0xf) + m_white*16; // bg blk -> bg wht
            else if ((print_col&0xf0) == (m_white*16)) print_col = (print_col&0xf) + m_black*16; // bg wht -> bg blk
          col_set = 0;
          if (out_mode[tmp]&mode_dim) col_set = 16;
          if (out_mode[tmp]&mode_bright) col_set = 8;

          printf("</b><b style=\"color: %s; background: %s;",
                 colour_names[(print_col&0xf)+col_set], colour_names[(print_col&0xf0)/16]);
          if ((out_mode[tmp]&mode_underscore) || (out_mode[tmp]&mode_blink))
            {
            printf("text-decoration:%s%s;",
                   ((out_mode[tmp]&mode_underscore)? " underline" : ""),
                   ((out_mode[tmp]&mode_blink)     ? " blink"     : "") );
            }
          if (out_mode[tmp]&mode_bright) printf("font-weight: bold;"); // Firefox messes this up

          printf("\">");
          current_mode = out_mode[tmp];
          current_col =  out_col[tmp];
        }
      
      if (!((wide_set||tall_set)&&(cX>(max_cX/2)))) {
        
        if (html_mode) {
          if (out_mode[tmp]&mode_hidden) printf(" ");
          else if (out[tmp] == '<') printf("&lt;");
          else if (out[tmp] == '>') printf("&gt;");
          else if (out_mode[tmp]&mode_graphics)
            if ( (out[tmp] >= 0x5f) && (out[tmp] < 0x7f) )
              printf("%s", graphics_chars[out[tmp]-0x5f]);
            else
              printf("%c" , out[tmp]);
          else
          printf("%c" , out[tmp]);
        }
        else
          if (out_mode[tmp]&mode_hidden) printf(" "); else printf("%c" , out[tmp]);
        
        if (wide_set&&(!tall_set)) if (!html_mode) printf(" "); else printf(" ");//printf("&nbsp;");
      }
    }
    if (tall_set && html_mode) printf("</b></span><b>");
    printf("\n");
  
}
