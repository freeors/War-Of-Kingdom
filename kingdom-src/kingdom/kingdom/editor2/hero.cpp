#include "hero.hpp"

bool hero::check_valid() const
{
	for (int i = 0; i < HEROS_MAX_PARENT; i ++) {
		if (number_ < hero::number_normal_min) {
			if (parent_[i].hero_ != HEROS_INVALID_NUMBER) {
				return false;
			}
		} else if (parent_[i].hero_ < hero::number_normal_min) {
			return false;
		}
	}

	for (int i = 0; i < HEROS_MAX_CONSORT; i ++) {
		if (number_ < hero::number_normal_min) {
			if (consort_[i].hero_ != HEROS_INVALID_NUMBER) {
				return false;
			}
		} else if (consort_[i].hero_ < hero::number_normal_min) {
			return false;
		}
	}

	for (int i = 0; i < HEROS_MAX_OATH; i ++) {
		if (number_ < hero::number_normal_min) {
			if (oath_[i].hero_ != HEROS_INVALID_NUMBER) {
				return false;
			}
		} else if (oath_[i].hero_ < hero::number_normal_min) {
			return false;
		}
	}

	for (int i = 0; i < HEROS_MAX_INTIMATE; i ++) {
		if (number_ < hero::number_normal_min) {
			if (intimate_[i].hero_ != HEROS_INVALID_NUMBER) {
				return false;
			}
		} else if (intimate_[i].hero_ < hero::number_normal_min) {
			return false;
		}
	}

	for (int i = 0; i < HEROS_MAX_HATE; i ++) {
		if (number_ < hero::number_normal_min) {
			if (hate_[i].hero_ != HEROS_INVALID_NUMBER) {
				return false;
			}
		} else if (hate_[i].hero_ < hero::number_normal_min) {
			return false;
		}
	}
	return true;
}

bool hero::confirm_carry_to(hero& h2, int carry_to)
{
	return true;
}