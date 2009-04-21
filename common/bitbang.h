#ifndef DEBRICK_BITBANG_H_
#define DEBRICK_BITBANG_H_

struct debrick_bitbang {
	unsigned int instruction_length;
	unsigned int tck_delay;
	int use_wiggler;
	int use_ludicrous_speed;
	int ludicrous_speed_corruption;
	void *parport_priv;

	int curinstr;
};

#endif /* DEBRICK_BITBANG_H_ */
