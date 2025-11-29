/**
 * @file	np2interop.cpp
 * @brief	Implementation of np2 <-> mame opl 
 */

#include "compiler.h"
#include "ymfm_opl.h"
#include "np2interop.h"

template<typename ChipType>
class chip_wrapper : public ymfm::ymfm_interface
{
public:
	int m_fmrate;
	int m_playrate;
	double m_fmcounter_rem;
	INT16 m_lastsample[4];

	chip_wrapper() :
		m_chip(*this),
		m_fmrate(0),
		m_playrate(0),
		m_fmcounter_rem(0),
		m_lastsample()
	{
		m_chip.reset();
	}

	ChipType& GetChip() {
		return m_chip;
	}

private:
	ChipType m_chip;
};


void* YMF262Init(int clock, int rate)
{
	chip_wrapper<ymfm::ymf262>* chip = new chip_wrapper<ymfm::ymf262>();
	if (!chip)
		return NULL;

	/* clear */
	ymfm::ymf262& chipcore = chip->GetChip();
	chip->m_fmrate = chipcore.sample_rate(clock);
	chip->m_playrate = rate;

	// reset
	chipcore.reset();

	return chip;
}

void YMF262Shutdown(void* chipptr)
{
	if (!chipptr) return;

	chip_wrapper<ymfm::ymf262>* chip = (chip_wrapper<ymfm::ymf262>*)chipptr;

	delete chip;
}
void YMF262ResetChip(void* chipptr)
{
	if (!chipptr) return;

	chip_wrapper<ymfm::ymf262>* chip = (chip_wrapper<ymfm::ymf262>*)chipptr;
	ymfm::ymf262& chipcore = chip->GetChip();

	chipcore.reset();
}

int YMF262Write(void* chipptr, int a, int v)
{
	if (!chipptr) return 0;

	chip_wrapper<ymfm::ymf262>* chip = (chip_wrapper<ymfm::ymf262>*)chipptr;
	ymfm::ymf262& chipcore = chip->GetChip();

	chipcore.write(a, v);

	return chipcore.read_status() >> 7;
}

unsigned char YMF262Read(void* chipptr, int a)
{
	if (!chipptr) return 0;

	chip_wrapper<ymfm::ymf262>* chip = (chip_wrapper<ymfm::ymf262>*)chipptr;
	ymfm::ymf262& chipcore = chip->GetChip();

	return chipcore.read(a);
}

int YMF262FlagSave(void* chipptr, void* dstbuf)
{
	if (!chipptr) return 0;

	chip_wrapper<ymfm::ymf262>* chip = (chip_wrapper<ymfm::ymf262>*)chipptr;
	ymfm::ymf262& chipcore = chip->GetChip();

	// 保存
	std::vector<uint8_t> buffer;
	ymfm::ymfm_saved_state saver(buffer, true);
	chipcore.save_restore(saver);
	const size_t bufsize = buffer.size();
	if (dstbuf != NULL){
		if (bufsize > 0) {
			memcpy(dstbuf, &(buffer[0]), bufsize);
		}
		*((int*)((uint8_t*)dstbuf + bufsize)) = chip->m_fmrate;
		*((int*)((uint8_t*)dstbuf + bufsize + sizeof(int))) = chip->m_playrate;
	}

	return bufsize + sizeof(int) * 2;
}
int YMF262FlagLoad(void* chipptr, void* srcbuf, int size)
{
	if (!chipptr) return 0;

	chip_wrapper<ymfm::ymf262>* chip = (chip_wrapper<ymfm::ymf262>*)chipptr;
	ymfm::ymf262& chipcore = chip->GetChip();

	if (srcbuf == NULL) return 0;

	// バッファサイズを取得
	std::vector<uint8_t> dummybuffer;
	ymfm::ymfm_saved_state saver(dummybuffer, true);
	chipcore.save_restore(saver);
	const size_t bufsize = dummybuffer.size();

	// バッファサイズがあっていないならエラー
	if (size != bufsize + sizeof(int) * 2) return 0;

	// 復元実行
	std::vector<uint8_t> buffer((uint8_t*)srcbuf, (uint8_t*)srcbuf + bufsize);
	ymfm::ymfm_saved_state restorer(buffer, false);
	chipcore.save_restore(restorer);
	chip->m_fmrate = *((int*)((uint8_t*)srcbuf + bufsize));
	chip->m_playrate = *((int*)((uint8_t*)srcbuf + bufsize + sizeof(int)));

	// 初期化
	memset(chip->m_lastsample, 0, sizeof(chip->m_lastsample));
	chip->m_fmcounter_rem = 0;

	return size;
}

#define OPL3_VOLUME_ADJUST	2
void YMF262UpdateOne(void* chipptr, INT16** buffers, int length)
{
	if (!chipptr) return;

	chip_wrapper<ymfm::ymf262>* chip = (chip_wrapper<ymfm::ymf262>*)chipptr;
	ymfm::ymf262& chipcore = chip->GetChip();

	if (buffers == NULL) return;

	const double fmlengthf = chip->m_fmcounter_rem + (double)length * chip->m_fmrate / chip->m_playrate;
	const int fmlength = (int)fmlengthf;
	chip->m_fmcounter_rem = fmlengthf - fmlength;

	if (fmlength > 0) {
		ymfm::ymf262::output_data* output = (ymfm::ymf262::output_data*)malloc(sizeof(ymfm::ymf262::output_data) * fmlength);
		if (output == NULL) return;

		chipcore.generate(output, fmlength);
		for (int i = 0; i < length; i++) {
			int srcIndex = (int)((double)i * chip->m_fmrate / chip->m_playrate);
			buffers[0][i] = output[srcIndex].data[0] / OPL3_VOLUME_ADJUST;
			buffers[1][i] = output[srcIndex].data[1] / OPL3_VOLUME_ADJUST;
			buffers[2][i] = output[srcIndex].data[2] / OPL3_VOLUME_ADJUST;
			buffers[3][i] = output[srcIndex].data[3] / OPL3_VOLUME_ADJUST;
		}
		chip->m_lastsample[0] = output[fmlength - 1].data[0];
		chip->m_lastsample[1] = output[fmlength - 1].data[1];
		chip->m_lastsample[2] = output[fmlength - 1].data[2];
		chip->m_lastsample[3] = output[fmlength - 1].data[3];

		free(output);
	}
	else {
		for (int i = 0; i < length; i++) {
			buffers[0][i] = chip->m_lastsample[0] / OPL3_VOLUME_ADJUST;
			buffers[1][i] = chip->m_lastsample[1] / OPL3_VOLUME_ADJUST;
			buffers[2][i] = chip->m_lastsample[2] / OPL3_VOLUME_ADJUST;
			buffers[3][i] = chip->m_lastsample[3] / OPL3_VOLUME_ADJUST;
		}
	}
}

