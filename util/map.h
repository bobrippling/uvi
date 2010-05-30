#ifndef MAP_H
#define MAP_H

typedef struct
	{
		struct list *pairs;
	} map;

/* maps a string onto anything */

map		*map_new(void);
void	map_add( map *, void *const, void *const);
void	*map_get(map *, void *const);
void	map_free(map *);


#endif
