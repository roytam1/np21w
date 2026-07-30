
#ifdef __cplusplus
extern "C" {
#endif

BRESULT	fdd_set_nfd(FDDFILE fdd, FDDFUNC fdd_fn, const OEMCHAR *fname, int ro);

BRESULT fdd_seeksector_nfd(FDDFILE fdd);	//	í«â¡(kaiE)
BRESULT	fdd_read_nfd(FDDFILE fdd);
BRESULT	fdd_write_nfd(FDDFILE fdd);
BRESULT fdd_readid_nfd(FDDFILE fdd);
BRESULT fdd_formatinit_nfd(FDDFILE fdd);	/* 170107 to support format command */

BRESULT fdd_seeksector_nfd1(FDDFILE fdd);	//	í«â¡(kaiD)
BRESULT	fdd_read_nfd1(FDDFILE fdd);			//	í«â¡(kaiD)
BRESULT	fdd_readdiag_nfd1(FDDFILE fdd);		//	NFD r1ì¡éÍì«Ç›çûÇ›
BRESULT	fdd_write_nfd1(FDDFILE fdd);		//	í«â¡(kaiD)
BRESULT fdd_readid_nfd1(FDDFILE fdd);		//	í«â¡(kaiD)

#ifdef __cplusplus
}
#endif

