//  Written by cshore (Daniel Dickinson) (although he hates to admit it)
//  cshore@bmts.com
// **************************************************************************
//
//  This program is copyright (C) 2009 Daniel Dickinson
//  This program is free software; you can redistribute it and/or modify it
//  under the terms of version 2 the GNU General Public License as published
//  by the Free Software Foundation.
//  This program is distributed in the hope that it will be useful, but WITHOUT
//  ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
//  FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
//  more details.
//  To view a copy of the license go to:
//  http://www.fsf.org/copyleft/gpl.html
//  To receive a copy of the GNU General Public License write the Free Software
//  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.
//
#include <stdio.h>
#include <errno.h>

int main(void) {
  int a[4], i;
  int done = 0;
  while(!done) {
    for (i = 0; i < 4; i++) {
      a[i] = getchar();
      if (a[i] == EOF) {
	a[i] = 0;
	if (!done) {
	  done = 1 + i;
	}       
      }
    }
    for (i = 3 ; i >= 0; i--) {
      if (done) {
	if ((done - 1) <= i ) {
	  continue;
	}
      }
      putchar(a[i]);
    }            
  }
  fprintf(stderr, "\nDone\n");
  return 0;
}
