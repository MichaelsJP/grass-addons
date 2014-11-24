#include <unistd.h>
#include <fcntl.h>
#include <grass/segment.h>
#include <grass/glocale.h>
#include "seg.h"

int dseg_open(DSEG *dseg, int srows, int scols, int nsegs_in_memory)
{
    char *filename;
    int errflag;
    int fd;

    dseg->filename = NULL;
    dseg->fd = -1;
    dseg->name = NULL;
    dseg->mapset = NULL;

    filename = G_tempfile();
    if (-1 == (fd = creat(filename, 0666))) {
	G_warning(_("dseg_open(): unable to create segment file"));
	return -2;
    }
    if (0 >
	(errflag =
	 Segment_format(fd, Rast_window_rows(), Rast_window_cols(), srows, scols,
			sizeof(DCELL)))) {
	close(fd);
	unlink(filename);
	if (errflag == -1) {
	    G_warning(_("Unable to write segment file"));
	    return -1;
	}
	else {
	    G_warning(_("Iillegal configuration parameter(s)"));
	    return -3;
	}
    }
    close(fd);
    if (-1 == (fd = open(filename, 2))) {
	unlink(filename);
	G_warning(_("Unable to re-open segment file"));
	return -4;
    }
    if (0 > (errflag = Segment_init(&(dseg->seg), fd, nsegs_in_memory))) {
	close(fd);
	unlink(filename);
	if (errflag == -1) {
	    G_warning(_("Unable to read segment file"));
	    return -5;
	}
	else {
	    G_warning(_("Out of memory"));
	    return -6;
	}
    }
    dseg->filename = filename;
    dseg->fd = fd;
    return 0;
}

int dseg_close(DSEG *dseg)
{
    Segment_release(&(dseg->seg));
    close(dseg->fd);
    unlink(dseg->filename);
    if (dseg->name) {
	G_free(dseg->name);
	dseg->name = NULL;
    }
    if (dseg->mapset) {
	G_free(dseg->mapset);
	dseg->mapset = NULL;
    }
    return 0;
}

int dseg_put(DSEG *dseg, DCELL *value, int row, int col)
{
    if (Segment_put(&(dseg->seg), (DCELL *) value, row, col) < 0) {
	G_warning(_("Unable to write segment file"));
	return -1;
    }
    return 0;
}

int dseg_put_row(DSEG *dseg, DCELL *value, int row)
{
    if (Segment_put_row(&(dseg->seg), (DCELL *) value, row) < 0) {
	G_warning(_("Unable to write segment file"));
	return -1;
    }
    return 0;
}

int dseg_get(DSEG *dseg, DCELL *value, int row, int col)
{
    if (Segment_get(&(dseg->seg), (DCELL *) value, row, col) < 0) {
	G_warning(_("Unable to read segment file"));
	return -1;
    }
    return 0;
}

int dseg_read_raster(DSEG *dseg, char *map_name, char *mapset)
{
    int row, nrows;
    int map_fd;
    DCELL *dbuffer;

    dseg->name = NULL;
    dseg->mapset = NULL;

    map_fd = Rast_open_old(map_name, mapset);
    nrows = Rast_window_rows();
    dbuffer = Rast_allocate_d_buf();
    for (row = 0; row < nrows; row++) {
	Rast_get_d_row(map_fd, dbuffer, row);
	if (Segment_put_row(&(dseg->seg), (DCELL *) dbuffer, row) < 0) {
	    G_free(dbuffer);
	    Rast_close(map_fd);
	    G_warning(_("Inable to segment put row %d for raster <%s>"),
                      row, map_name);
	    return -1;
	}
    }

    Rast_close(map_fd);
    G_free(dbuffer);

    dseg->name = G_store(map_name);
    dseg->mapset = G_store(mapset);

    return 0;
}

int dseg_write_cellfile(DSEG *dseg, char *map_name)
{
    int map_fd;
    int row, nrows;
    DCELL *dbuffer;

    map_fd = Rast_open_new(map_name, DCELL_TYPE);
    nrows = Rast_window_rows();
    dbuffer = Rast_allocate_d_buf();
    Segment_flush(&(dseg->seg));
    for (row = 0; row < nrows; row++) {
	G_percent(row, nrows, 1);
	Segment_get_row(&(dseg->seg), (DCELL *) dbuffer, row);
	Rast_put_row(map_fd, dbuffer, DCELL_TYPE);
    }
    G_percent(row, nrows, 1);    /* finish it */
    G_free(dbuffer);
    Rast_close(map_fd);
    return 0;
}
