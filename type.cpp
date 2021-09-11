//include section
#include<ctype.h>
#include<cstdio>
#include<errno.h>
#include<iostream>
#include<termios.h>
#include<string.h>
#include<stdlib.h>
#include<sys/ioctl.h>
#include<unistd.h>

#define type_VERSION "0.0.1"
#define CTRL_KEY(k) ((k) & 0x1f)
#define ABUF_INIT {NULL,0}

/****  append buffer ****/

enum editorKey{
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





//data section 

struct editorConfig{

    int cx,cy;
    int screenrows;
    int screencols;

struct termios orig_termios;

};

struct editorConfig E;

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

void editorMoveCursor(int key)
{
    switch(key)
    {
        case ARROW_LEFT:
        if(E.cx!=0)
        {
            E.cx--;
        }
        break;

        case ARROW_RIGHT:
        if(E.cx != E.screencols -1 )
        {
            E.cx++;
        }
        break;

        case ARROW_UP:
        if(E.cy != 0)
        {
            E.cy--;
        }
        break;

        case ARROW_DOWN:
        if(E.cy != E.screenrows-1)
        {
            E.cy++;
        }
        break;
    }
}

void editorProcessKeypress()
{
    int c= editorReadkey();

    switch(c)
    {
        case CTRL_KEY('q'):
        write(STDOUT_FILENO,"\x1b[2J",4);
        write(STDOUT_FILENO,"\x1b[H",3);
        exit(0);
        break;

        case HOME_KEY:
        E.cx=0;
        break;

        case END_KEY:
        E.cx = E.screencols-1;
        break;



        case PAGE_UP:
        case PAGE_DOWN:
        {
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


    }
}

void editorDrawRows(struct appendbuf *ab)
{
    int y;
    for(y=0;y<E.screenrows;y++)
    {
        if(y == E.screenrows /3)
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

        abufappend(ab,"\x1b[K",3);
        if(y<E.screenrows-1)
        {
            abufappend(ab,"\r\n",2);

        }
    
    }
}

void editorrefreshScreen()
{
    struct appendbuf ab =ABUF_INIT;

    abufappend(&ab,"\x1b[?25l",6);
    // abufappend(&ab,"\x1b[2J",4);
    abufappend(&ab,"\x1b[H",3);

    editorDrawRows(&ab);

    char buf[32];
    snprintf(buf,sizeof(buf),"\x1b[%d;%dH",E.cy+1,E.cx+1);
    abufappend(&ab,buf,strlen(buf));

    abufappend(&ab,"\x1b[?25h",6);

    write(STDOUT_FILENO,ab.b,ab.len);
    abFree(&ab);
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

    if(getWindowSize(&E.screenrows,&E.screencols)== -1 ) die("getWindowSize");
}
//init 

int main()
{
   
    // std::cout<<"        ......././././.. Thanks for Downlaoding type Text editor ...././././././";

    enableRawMode();
    initEditor();


    
    while(1){
        editorrefreshScreen();

      editorProcessKeypress();

    }

    return 0;
    
}