#define _DEFAULT_SOURCE
#define _BSD_SOURCE
// #define _GNU_SOURCE 

//include section
#include<ctype.h>
#include<stdio.h>
#include<errno.h>
#include<fcntl.h>
#include<iostream>
#include<termios.h>
#include<time.h>
#include<stdarg.h>
#include<string.h>
#include<stdlib.h>
#include<sys/types.h>
#include<sys/ioctl.h>
#include<unistd.h>


#define type_QUIT_TIMES 3
#define type_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT {NULL,0}
#define type_TAB_STOP 8

/****  append buffer ****/

enum editorKey{
    BACKSPACE=127,
    ARROW_LEFT =1000,
    ARROW_RIGHT,
    ARROW_UP,
    ARROW_DOWN,
    DEL_KEY,
    HOME_KEY,
    END_KEY,
    PAGE_UP,
    PAGE_DOWN
};






struct appendbuf{
    char *b;
    int len;

};
void abufappend(struct appendbuf *ab,const char *s,int len)

{
char *New = (char*)realloc(ab->b,ab->len +len);
if(New  == NULL) return;
memcpy(&New[ab->len],s,len); // main line is these 
ab->b = New;
ab->len += len;
}

void abFree(struct appendbuf *ab)
{
    free(ab->b);
}





//data section ----------------------------------------------- erow struct here -------------------

typedef struct erow{
    int size;
    int rsize;
    char *chars;
    char *render;


} erow;


struct editorConfig{

    int cx,cy;
    int rx;
    int rowoff;
    int coloff;
    int screenrows;
    int screencols;
    int numrows;
    erow *row;
    int dirty;
    char *filename;
    char statusmsg[80];
    time_t statusmsg_time;

    struct termios orig_termios;

};


struct editorConfig E;

/// _--------------------> prototypes funciton <--------------------------------------------------
void editorSetstatusMessage(const char *fmt, ...);
void editorrefreshScreen();
char *editorPrompt(char *prompt);


//terminal section 

void die(const char* s)
{
    write(STDOUT_FILENO,"\x1b[2J",4);
    write(STDOUT_FILENO,"\x1b[H",3);
    perror(s);
    exit(1);

}

void disableRawMode()
{
    if( tcsetattr(STDIN_FILENO,TCSAFLUSH, &E.orig_termios) ==-1)
    {
        die("tcsetattr");

    }
    // std::cout<<"thank for using"; --------------------------> optional for using atexit in some way 


}

//file i/o opertions /////

void editorUpdateRow(erow *row)
{
    int tabs =0;
    int j;
    for(j=0;j<row->size;j++)
    {
        if(row->chars[j]=='\t') tabs++;

    }


    free(row->render);
    row->render= (char* )malloc(row->size +tabs*(type_TAB_STOP -1 ) +1);
    int idx =0;
    for (j=0;j<row->size;j++)
    {
        if(row->chars[j] =='\t')
        {
            row->render[idx++]=' ';
            while(idx %type_TAB_STOP != 0 ) row->render[idx++]= ' ';

        }
        else {

            row->render[idx++] =row->chars[j];
        }
    }
    row->render[idx]='\0';
    row->rsize=idx;

}

void editorAppendRow(int at,const char *s,size_t len) // ------------------> change const ot c <------------
{
    if(at<0 || at>E.numrows) return;

     E.row=(erow*)realloc(E.row,sizeof(erow)*(E.numrows+1));
     memmove(&E.row[at+1],&E.row[at],sizeof(erow)*(E.numrows -at ));


    // int at= E.numrows; // change to find the bug an fixes

 
    E.row[at].size= len;
    E.row[at].chars=(char*)malloc(len+1);
    memcpy(E.row[at].chars,s,len);
    E.row[at].chars[len]= '\0';

    E.row[at].rsize=0;
    E.row[at].render=NULL;
    editorUpdateRow(&E.row[at]);


    E.numrows++;
    E.dirty++;

    
}

void editorFreeRow(erow *row)
{
    free(row->render);
    free(row->chars);

}

void editorDelRow(int at)
{
    if(at<0 || at>=E.numrows) return;
    editorFreeRow(&E.row[at]);
    memmove(&E.row[at],&E.row[at+1],sizeof(erow)*(E.numrows -at-1));
    E.numrows++;
    E.dirty++;

} 

void editorRowAppendString( erow *row,char *s,size_t len)
{
    row->chars= (char *) realloc(row->chars,row->size + len +1);
    memcpy(&row->chars[row->size],s,len);
    row->size+=len;
    row->chars[row->size]= '\0';
    editorUpdateRow(row);
    E.dirty++;

}

void editorRowInsertChar(erow *row,int at ,int c)
{
    if(at<0 || at > row->size) at = row->size;
    row->chars =(char* )realloc(row->chars,row->size +2 );
    memmove(&row->chars[at +1 ],&row->chars[at],row->size - at +1);
    row->size++;
    row->chars[at]= c;
    editorUpdateRow(row);
    E.dirty++;



}

void editorRowDelChar(erow *row, int at)
{
    if(at<0 || at >= row->size) return;
    memmove(&row->chars[at],&row->chars[at+1],row->size -at );
    row->size--;
    editorUpdateRow(row);
    E.dirty++;

}

void editorDelchar()
{
    if(E.cy== E.numrows) return ;
    if(E.cx == 0 && E.cy==0) return ;


    erow *row=&E.row[E.cy];
    if(E.cx>0)
    {
        editorRowDelChar(row,E.cx-1);
        E.cx--;

    }else{
        E.cx=E.row[E.cx-1].size;
        editorRowAppendString(&E.row[E.cy-1],row->chars,row->size);
        editorDelRow(E.cy);
        E.cy--;


    }

}

void editorInsertChar(int c )
{
    if(E.cy==E.numrows)
    {
        editorAppendRow(E.numrows,"",0);

    }
    editorRowInsertChar(&E.row[E.cy],E.cx,c);
    E.cx++;

}

void editorInsertNewline()
{
    if(E.cx==0)
    {
        editorAppendRow(E.cy,"",0);

    }else 
    {
        erow *row = &E.row[E.cy];
        editorAppendRow(E.cy +1,&row->chars[E.cx],row->size -E.cx);
        row= &E.row[E.cy];
        row->size=E.cx;
        row->chars[row->size]= '\0';
        editorUpdateRow(row);

    }
    E.cy++;
    E.cx=0;

}

char *editorRowToString(int *buflen)
{
    int totlen=0;
    int j;
    for(j=0;j<E.numrows;j++)
    {
        totlen +=E.row[j].size +1;

    }
    *buflen=totlen;


    char* buf = (char *)malloc(totlen);
    char *p=buf;
    for(j=0;j<E.numrows;j++)
    {
        memcpy(p,E.row[j].chars,E.row[j].size);
        p+=E.row[j].size;
        *p='\n';
        p++;

    }
    return buf;

}


int editorReadkey()
{
    int nread;
    char a;
    while((nread = read(STDIN_FILENO,&a,1 ))!=1)
    {
        if(nread == -1 && errno !=EAGAIN ) die ("read");

    }

    if(a=='\x1b')
    {
        char seq[3];

        if(read(STDIN_FILENO,&seq[0],1) !=1) return '\x1b';
        if(read(STDIN_FILENO,&seq[1],1) !=1) return '\x1b';

        if(seq[0] == '[')
        {
            if(seq[1] >='0' && seq[1]<='9')
            {
                if(read(STDIN_FILENO,&seq[2],1) != 1) return '\x1b';
                if(seq[2]=='~')
                {
                    switch(seq[1])
                    {
                        case '1': return HOME_KEY;
                        case '3': return DEL_KEY;
                        case '4': return END_KEY;
                        case '5' : return PAGE_UP;
                        case '6' : return PAGE_DOWN;
                        case '7' : return HOME_KEY;
                        case '8' : return END_KEY;
                    }
                }
            }
            else 
            { 
                switch(seq[1])
                {
                    case 'A' : return  ARROW_UP;
                    case 'B' : return  ARROW_DOWN;
                    case 'C' : return  ARROW_RIGHT;
                    case 'D' : return  ARROW_LEFT;
                    case 'H' : return HOME_KEY;
                    case 'F' : return END_KEY;
                }
            }
        }
            else if(seq[0]=='O')
            {
                switch(seq[1])
                {
                    case 'H' : return HOME_KEY;
                    case 'F' : return END_KEY;
                }
            }
        

        return '\x1b';
    }else 
    {
        return a;
    }
}

void editorOpen( char *filename)
{
    free(E.filename);  // _________________---------------------------> error came from here of that free pointer error which came at 13/09/2021  
    E.filename=strdup(filename);


//   const char *line ="Hello world!"; // ---------> maybe can cause error 
    FILE *fp=fopen(filename,"r");
    if(!fp) die("fopen");

    char *line=NULL;
    size_t linecap=0; // ------------------->

    ssize_t linelen;
    while((linelen = getline(&line,&linecap,fp))!= -1)
    {
      while(linelen > 0 && (line[linelen -1 ] =='\n' || line[linelen-1] == '\r'))
            linelen--;
    editorAppendRow(E.numrows,line,linelen);

    
     
    }
    free(line);
    free(fp);
    E.dirty=0;


}

char *editorPrompt(const char *prompt)
{
    size_t bufsize= 128;
    char *buf = (char *)malloc(sizeof(bufsize));

    size_t buflen=0;
    buf[0]='\0';

    while(1)
    {
        editorSetstatusMessage(prompt,buf);
        editorrefreshScreen();

        int c=editorReadkey();
      if(c==DEL_KEY || c== CTRL_KEY('h') || c==BACKSPACE)
      {
          if(buflen != 0) buf[--buflen]= '\0';

      }
      else if(c=='\x1b')
       {
           editorSetstatusMessage("");
           free(buf);
           return NULL;
       }
        else if(c=='\r')
        {
            if(buflen !=0 )
            {
                editorSetstatusMessage("");
                return buf;

            }

        }else if(!iscntrl(c) && c<128) {
            if(buflen == bufsize -1)
            {
                bufsize*=2;
                buf=(char *)realloc(buf,bufsize);


            }
            buf[buflen++]= c ;
            buf[buflen] = '\0';


        }
    }
}


void editorSave()
{
    if(E.filename == NULL) // name  indicate the function. It is stand for filename is exist or not
    {
        E.filename= editorPrompt("Save as:<< %s >> (ESC to cancel)");
        if(E.filename == NULL)
        {
            editorSetstatusMessage("Save aborted");
            return;
        }

    }
    int len ;
    char *buf = editorRowToString(&len);

    int fd= open(E.filename,O_RDWR | O_CREAT,0644);
    if (fd != -1 )
    {
        if(ftruncate(fd,len)!=-1)
        {
            if(write(fd,buf,len)==len)
            {
                close(fd);
                free(buf);
                E.dirty=0;

                editorSetstatusMessage("%d bytes written to disk",len);
                return;

            }
        }
        close(fd);

    }
    free(buf);
    editorSetstatusMessage("Can't save ! I/O error : %s",strerror(errno));



}

// _------------> editor save as funciton is here naming is crazy but still understood


void enableRawMode(){

    if (tcgetattr(STDIN_FILENO,&E.orig_termios)== -1)
    {
        die("tcgetattr");

    }
    atexit(disableRawMode);


    struct termios raw= E.orig_termios;
    raw.c_iflag &=~(IXON | ICRNL|BRKINT|INPCK);
    raw.c_oflag &=~(OPOST);
    raw.c_cflag |= (CS8);


    raw.c_lflag &= ~(ECHO | ICANON| IEXTEN | ISIG);
    raw.c_cc[VMIN]=0;
    raw.c_cc[VTIME]=1;

   if ( tcsetattr(STDIN_FILENO,TCSAFLUSH,&raw)==-1 ) die("tcsetattr");


}

void editorMoveCursor(int key)
{
    erow *row= (E.cy >= E.numrows) ? NULL:&E.row[E.cy];

    switch(key)
    {
        case ARROW_LEFT:
        if(E.cx!=0)
        {
            E.cx--;
        }
        else if(E.cy >0){   
            E.cy--;
            E.cx= E.row[E.cy].size;
        }
        break;

        case ARROW_RIGHT:
        if(row && E.cx < row->size)
        {
            E.cx++;
        }
        else if(row && E.cx == row->size)
        {
            E.cy++;
            E.cx = 0;
        }
        break;

        case ARROW_UP:
        if(E.cy != 0)
        {
            E.cy--;
        }
        break;

        case ARROW_DOWN:
        if(E.cy <E.numrows)
        {
            E.cy++;
        }
        break;
    }
    row= (E.cy >= E.numrows)? NULL:&E.row[E.cy];
    int rowlen= row? row->size : 0;
    if(E.cx > rowlen)
    {
        E.cx= rowlen;

    }
}

void editorProcessKeypress()
{
    static int quit_times = type_QUIT_TIMES;
    int c= editorReadkey();

    switch(c)
    {
        case '\r':
        editorInsertNewline();

        
        break;


        case CTRL_KEY('q'):
        if(E.dirty && quit_times > 0)
        {
            editorSetstatusMessage("WARNING !!! file has unsaved changes. ","Press CTRL+q %d more times to quit.",quit_times);
            quit_times--;
            return;

        }


        write(STDOUT_FILENO,"\x1b[2J",4);
        write(STDOUT_FILENO,"\x1b[H",3);
        exit(0);
        break;

        case CTRL_KEY('s'):
        editorSave();
        break;


        case HOME_KEY:
        E.cx=0;
        break;

        case END_KEY:
            if(E.cy < E.numrows)
            {
                E.cx = E.row[E.cy].size;

            }
        break;

        case BACKSPACE:
        case CTRL_KEY('h'):
        case DEL_KEY:
        if(c==DEL_KEY) editorMoveCursor(ARROW_RIGHT);
        editorDelchar();

        break;




        case PAGE_UP:
        case PAGE_DOWN:
        {
            if(c==PAGE_UP)
            {
                E.cy=E.rowoff;
            }else if(c==PAGE_DOWN)
            {
                E.cy=E.rowoff+E.screenrows-1;
                if(E.cy>E.numrows) E.cy=E.numrows;

            }

            int times=E.screenrows;
            while(times--)
                editorMoveCursor(c==PAGE_UP ? ARROW_UP : ARROW_DOWN);
        }
        break;


        case ARROW_UP:
        case ARROW_DOWN:
        case ARROW_LEFT:
        case ARROW_RIGHT:
            editorMoveCursor(c);
            break;
            
        
        case CTRL_KEY('l'):
        case '\x1b':
        break;


        default:
        editorInsertChar(c);
        break;


    }
    quit_times=type_QUIT_TIMES;

}

void editorDrawRows(struct appendbuf *ab)
{
    int y;
    for(y=0;y<E.screenrows;y++)
    {
        int filerow=y+E.rowoff;
            if(filerow>=E.numrows){
                if(E.numrows==0 &&  y == E.screenrows /3)
                {
                    char welcome[80];
                    int welcomelen= snprintf(welcome,sizeof(welcome),"type editor --version %s",type_VERSION);
                    if(welcomelen>E.screencols) welcomelen = E.screencols;
                    int padding = (E.screencols- welcomelen)/2;
                    if(padding)
                    {
                        abufappend(ab,"~",1);
                        padding--;

                    }
                    while(padding--) abufappend(ab," ",1);
                    abufappend(ab,welcome,welcomelen);


                }else{
                    abufappend(ab,"~",1);
                }
            }else 
            {
                int len=E.row[filerow].rsize - E.coloff;
                if(len< 0) len =0;
                if(len>E.screencols) len = E.screencols;
                abufappend(ab,&E.row[filerow].render[E.coloff],len); // --------------------------------> try to figure it out something went crazy here

            }

        abufappend(ab,"\x1b[K",3);
        // if(y<E.screenrows-1)
        
            abufappend(ab,"\r\n",2);

        
    
    }
}

//_--------------------------------------- output---------------------------

int editorRowCxtoRx(erow *row,int cx)
{
    int rx=0;
    int j;
    for (j=0;j<cx;j++)
    {
        if(row->chars[j] == '\t')
        {
            rx+=(type_TAB_STOP-1) - (rx % type_TAB_STOP);

        }
        rx++;

    }
    return rx;

}

void editorscroll()
{
    E.rx=0;
    if(E.cy<E.numrows)
    {
        E.rx= editorRowCxtoRx(&E.row[E.cy],E.cx);

    }

    if(E.cy<E.rowoff)
    {
        E.rowoff= E.cy;

    }
    if(E.cy >= E.rowoff + E.screenrows)
    {
        E.rowoff= E.cy - E.screenrows+1;

    }
    if(E.rx < E.coloff)
    {
        E.coloff=E.rx;

    }
    if(E.rx >=E.coloff + E.screencols)
    {
        E.coloff= E.rx- E.screencols +1;
        
    }
}

void editorDrawStatusBar(struct appendbuf *ab)
{
    abufappend(ab,"\x1b[7m",4);

    char status[80],rstatus[80];

    int len =snprintf(status,sizeof(status),"%.20s - %d lines %s",E.filename ? E.filename :"[No Name]",E.numrows,E.dirty ? "(modified)" : "");

    int rlen= snprintf(rstatus,sizeof(rstatus),"%d/%d",E.cy+1,E.numrows);


    if(len>E.screencols) len =E.screencols;
    abufappend(ab,status,len);

    while(len<E.screencols)
    {
        if(E.screencols-len == rlen)
        {
            abufappend(ab,rstatus,rlen);
            break;
        }else{

            abufappend(ab," ",1);
            len++;
        }

    }
    abufappend(ab,"\x1b[m",3);
    abufappend(ab,"\r\n",2);


}

void editorDrawMassageBar(struct appendbuf *ab)
{
    abufappend(ab,"\x1b[K",3);
    int msglen = strlen(E.statusmsg);
    if(msglen > E.screencols) msglen = E.screencols;
    if(msglen && time(NULL) - E.statusmsg_time<5)
        {
            abufappend(ab,E.statusmsg,msglen);
        }

}

void editorrefreshScreen()
{
    editorscroll();

    struct appendbuf ab =ABUF_INIT;

    abufappend(&ab,"\x1b[?25l",6);
    // abufappend(&ab,"\x1b[2J",4);
    abufappend(&ab,"\x1b[H",3);

    editorDrawRows(&ab);
    editorDrawStatusBar(&ab);
    editorDrawMassageBar(&ab);


    char buf[32];
    snprintf(buf,sizeof(buf),"\x1b[%d;%dH",(E.cy-E.rowoff)+1 ,(E.rx - E.coloff) +1);
    abufappend(&ab,buf,strlen(buf));

    abufappend(&ab,"\x1b[?25h",6);

    write(STDOUT_FILENO,ab.b,ab.len);
    abFree(&ab);
}

void editorSetstatusMessage(const char *fmt, ...)
{
    va_list ap;
    va_start(ap,fmt);
    vsnprintf(E.statusmsg,sizeof(E.statusmsg),fmt,ap);
    va_end(ap);
    E.statusmsg_time=time(NULL);


}

int getCursorPosition(int* rows ,int* cols)
{
    char buf[32];
    unsigned int i=0;

    if(write(STDOUT_FILENO,"\x1b[6n",4)!=4) return -1;

  while(i<sizeof(buf)-1)
  {
      if(read(STDIN_FILENO,&buf[i],1)!=1)break;
      if(buf[i]=='R') break;
      i++;

  }
buf[i]='\0';

if(buf[0] != '\x1b' || buf[1]!= '[') return -1;
if(sscanf(&buf[2], "%d;%d",rows,cols) !=2 )return -1;


    //   printf("\r\n&buf[1]: '%s'\r\n",&buf[1]);

    // editorReadkey();
    return 0;

}

int getWindowSize(int* rows,int* cols )
{
    struct winsize ws;
    if(ioctl(STDOUT_FILENO,TIOCGWINSZ,&ws)== -1 || ws.ws_col==0)
    {
        if(write(STDOUT_FILENO,"\x1b[999C\x1b[999B",12) != 12 ) return -1;
        // editorReadkey(); //  i think something will happen here <---------------------------------------------------------------
        getCursorPosition(rows,cols);
        return -1;

    }else{
        *cols=ws.ws_col;
        *rows=ws.ws_row;
        return 0;

    }
}




void initEditor()
{
    E.cx=0;
    E.cy=0;
    E.rx=0;
    E.rowoff=0;
    E.coloff=0;
    E.numrows=0;
    E.row=NULL;
    E.dirty=0;
    E.filename=NULL;
    E.statusmsg[0]='\0';
    E.statusmsg_time=0;

    
    if(getWindowSize(&E.screenrows,&E.screencols)== -1 ) die("getWindowSize");
    E.screenrows-=2;

}
//init 

int main( int argv , char *argc[])
{
   
    // std::cout<<"        ......././././.. Thanks for Downlaoding type Text editor ...././././././";

    enableRawMode();
    initEditor();
    if(argv>=2)
    {
        editorOpen(argc[1]);

    }


    editorSetstatusMessage("HELP:Ctrl+q = quit | Ctrl+S=save \n other command is still coming so wait for it.. hehe");

    while(1){
        editorrefreshScreen();

      editorProcessKeypress();

    }

    return 0;
    
}