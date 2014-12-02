#define GETTEXT_DOMAIN "wesnoth-lib"
#include "global.hpp"
#include <map>
#include <string>
#include <vector>

#include "config.hpp"
#include "filesystem.hpp"
#include "tstring.hpp"

// terrain_builder
#include "builder.hpp"
#include "image.hpp"

#include "map.hpp"

#include <boost/foreach.hpp>
#include "posix.h"

#define WMLBIN_MARK_CONFIG		"[cfg]"
#define WMLBIN_MARK_CONFIG_LEN	5
#define WMLBIN_MARK_VALUE		"[val]"
#define WMLBIN_MARK_VALUE_LEN	5

uint32_t tstring_textdomain_idx(const char *textdomain, std::vector<std::string> &td) 
{
	std::vector<std::string>::iterator iter = find(td.begin(), td.end(), textdomain);
	if (iter != td.end()) {
		return iter - td.begin() + 1;
	} else {
		return 0;
	}
}

// @deep: 嵌套深度, 顶层: 0
void wml_config_to_fp(posix_file_t fp, const config &cfg, uint32_t *max_str_len, std::vector<std::string> &td, uint16_t deep)
{
	uint32_t							u32n, bytertd;
	int									first;
		
	// config::child_list::const_iterator	ichildlist;
	// string_map::const_iterator			istrmap;

	// recursively resolve children
	BOOST_FOREACH (const config::any_child &value, cfg.all_children_range()) {
		// save {[cfg]}{len}{name}
		posix_fwrite(fp, WMLBIN_MARK_CONFIG, WMLBIN_MARK_CONFIG_LEN, bytertd);
		u32n = posix_mku32(value.key.size(), deep);
		posix_fwrite(fp, &u32n, sizeof(u32n), bytertd);
		posix_fwrite(fp, value.key.c_str(), posix_lo16(u32n), bytertd);

		*max_str_len = posix_max(*max_str_len, value.key.size());

		// save {[val]}{len}{name0}{len}{val0}{len}{name1}{len}{val1}{...}
		// string_map	&values = value.cfg.gvalues();
		// for (istrmap = values.begin(); istrmap != values.end(); istrmap ++) {
		first = 1;
		BOOST_FOREACH (const config::attribute &istrmap, value.cfg.attribute_range()) {
			if (first) {
				posix_fwrite(fp, WMLBIN_MARK_VALUE, WMLBIN_MARK_VALUE_LEN, bytertd);
				first = 0;
			}
			u32n = istrmap.first.size();
			posix_fwrite(fp, &u32n, sizeof(u32n), bytertd);
			posix_fwrite(fp, istrmap.first.c_str(), u32n, bytertd);
			*max_str_len = posix_max(*max_str_len, u32n);

			if (istrmap.second.t_str().translatable()) {
				// parse translatable string
				std::vector<t_string_base::trans_str> trans = istrmap.second.t_str().valuex();
				for (std::vector<t_string_base::trans_str>::const_iterator ti = trans.begin(); ti != trans.end(); ti ++) {
					if (ti == trans.begin()) {
						if (ti->td.empty()) {
							u32n = posix_mku32(0, posix_mku16(0, trans.size()));
						} else {
							u32n = posix_mku32(0, posix_mku16(tstring_textdomain_idx(ti->td.c_str(), td), trans.size()));
						}
					} else {
						if (ti->td.empty()) {
							u32n = posix_mku32(0, 0);
						} else {
							u32n = posix_mku32(0, posix_mku16(tstring_textdomain_idx(ti->td.c_str(), td), 0));
						}
					}
					// flag
					posix_fwrite(fp, &u32n, sizeof(u32n), bytertd);
					// length of value
					u32n = ti->str.size();
					posix_fwrite(fp, &u32n, sizeof(u32n), bytertd);
					posix_fwrite(fp, ti->str.c_str(), u32n, bytertd);
				}
			} else {
				// flag
				u32n = 0;
				posix_fwrite(fp, &u32n, sizeof(u32n), bytertd);
				// length of value
				u32n = istrmap.second.str().size();
				posix_fwrite(fp, &u32n, sizeof(u32n), bytertd);
				posix_fwrite(fp, istrmap.second.str().c_str(), u32n, bytertd);
			}
			*max_str_len = posix_max(*max_str_len, u32n);

		}		
		wml_config_to_fp(fp, value.cfg, max_str_len, td, deep + 1);
	}

	return;
}

// config就三个成员变量, values, children, orderer_children. orderer_child_ren可由children推出
// config被组织成一个树状结构
void wml_config_to_file(const std::string &fname, config &cfg, uint32_t nfiles, uint32_t sum_size, uint32_t modified)
{
	posix_file_t						fp = INVALID_FILE;
	uint32_t							max_str_len, bytertd, u32n; 

	std::vector<std::string>			tdomain;
	
	posix_print("<xwml.cpp>::wml_config_to_file------fname: %s\n", fname.c_str());

	posix_fopen(fname.c_str(), GENERIC_WRITE, CREATE_ALWAYS, fp);
	if (fp == INVALID_FILE) {
		posix_print("------<xwml.cpp>::wml_config_to_file, cannot create %s for wrtie\n", fname.c_str());
		goto exit;
	}
	// 

	max_str_len = posix_max(WMLBIN_MARK_CONFIG_LEN, WMLBIN_MARK_VALUE_LEN);
	posix_fseek(fp, 16 + sizeof(max_str_len) + sizeof(u32n), 0);

	// write [textdomain]
	BOOST_FOREACH (const config &d, cfg.child_range("textdomain")) {
		if (std::find(tdomain.begin(), tdomain.end(), d.get("name")->str()) != tdomain.end()) {
			continue;
		}
		tdomain.push_back(d.get("name")->str());
		u32n = tdomain.back().size();
		posix_fwrite(fp, &u32n, sizeof(u32n), bytertd);
		posix_fwrite(fp, tdomain.back().c_str(), u32n, bytertd);
	}

	wml_config_to_fp(fp, cfg, &max_str_len, tdomain, 0);

	// update max_str_len/textdomain_count
	posix_fseek(fp, 0, 0);

	// 0--15
	u32n = mmioFOURCC('X', 'W', 'M', 'L');
	posix_fwrite(fp, &u32n, 4, bytertd);
	posix_fwrite(fp, &nfiles, 4, bytertd);
	posix_fwrite(fp, &sum_size, 4, bytertd);
	posix_fwrite(fp, &modified, 4, bytertd);
	// 16--19(max_str_len)
	posix_fwrite(fp, &max_str_len, sizeof(max_str_len), bytertd);
	// 20--23(textdomain_count)
	u32n = tdomain.size();
	posix_fwrite(fp, &u32n, sizeof(u32n), bytertd);

	posix_print("------<xwml.cpp>::wml_config_to_file, return\n");
exit:
	if (fp != INVALID_FILE) {
		posix_fclose(fp);
	}

	return;
}


int wml_config_from_data(uint8_t *data, uint32_t datalen, uint8_t *namebuf, uint8_t *valbuf, std::vector<std::string> &tdomain, config &cfg)
{
	int									retval;
	uint8_t								*rdpos = data;
	uint32_t							u32n, len, transcnt, tdidx;
	uint16_t							deep;

	config::child_list					lastcfg;							

	retval = -1;

	lastcfg.push_back(&cfg);

	uint32_t max_str_len = 0;
	int debug = 0;
	while (rdpos < data + datalen) {

		// posix_print("in while, rdpos: %p, pos: %u(0x%x)", rdpos, rdpos - data + 4, rdpos - data + 4);
		// read {[cfg]}{len}{name}
		if (memcmp(rdpos, WMLBIN_MARK_CONFIG, WMLBIN_MARK_CONFIG_LEN)) {
			posix_print("mark is not %s\n", WMLBIN_MARK_CONFIG);
			goto exit;
		}
		rdpos = rdpos + WMLBIN_MARK_CONFIG_LEN;
		memcpy(&u32n, rdpos, sizeof(u32n));
		len = posix_lo16(u32n);
		deep = posix_hi16(u32n);
		rdpos = rdpos + sizeof(u32n);

		memcpy(namebuf, rdpos, len);
		namebuf[len] = 0;
		rdpos = rdpos + len;

		// posix_print("deep: %u, name: %s\n", deep, namebuf);

		config &cfgtmp = lastcfg[deep]->add_child(std::string((char *)namebuf));
		if (deep + 1 >= (uint16_t)lastcfg.size()) {
			lastcfg.push_back(&cfgtmp);
		} else {
            lastcfg[deep + 1] = &cfgtmp;
		}

		// read {[val]}{len}{name0}{len}{val0}{len}{name1}{len}{val1}{...}
		if (!memcmp(rdpos, WMLBIN_MARK_VALUE, WMLBIN_MARK_VALUE_LEN)) {
			// 存在value
			rdpos = rdpos + WMLBIN_MARK_VALUE_LEN;

			while ((rdpos < data + datalen) && memcmp(rdpos, WMLBIN_MARK_CONFIG, WMLBIN_MARK_CONFIG_LEN)) {
				// name
				memcpy(&len, rdpos, sizeof(len));
				rdpos = rdpos + sizeof(len);

				memcpy(namebuf, rdpos, len);
				namebuf[len] = 0;
				rdpos = rdpos + len;

				// value
				memcpy(&u32n, rdpos, sizeof(u32n));
				rdpos = rdpos + sizeof(u32n);
				
				transcnt = posix_hi8(posix_hi16(u32n));
				tdidx = posix_lo8(posix_hi16(u32n));

				memcpy(&len, rdpos, sizeof(len));
				rdpos = rdpos + sizeof(len);
				memcpy(valbuf, rdpos, len);
				valbuf[len] = 0;
				rdpos = rdpos + len;

				if (transcnt) {
					if (tdidx) {
						cfgtmp[std::string((char *)namebuf)] = t_string((const char *)valbuf, tdomain[tdidx - 1]);
					} else {
						cfgtmp[std::string((char *)namebuf)] = t_string((const char *)valbuf);
					}
					transcnt --;
					while (transcnt != 0) {
						// value
						memcpy(&u32n, rdpos, sizeof(u32n));
						rdpos = rdpos + sizeof(u32n);

						tdidx = posix_lo8(posix_hi16(u32n));

						memcpy(&len, rdpos, sizeof(len));
						rdpos = rdpos + sizeof(len);
						memcpy(valbuf, rdpos, len);
						valbuf[len] = 0;
						rdpos = rdpos + len;

						if (tdidx) {
							cfgtmp[std::string((char *)namebuf)] = cfgtmp[std::string((char *)namebuf)].t_str() + t_string((const char *)valbuf, tdomain[tdidx - 1]);
						} else {
							cfgtmp[std::string((char *)namebuf)] = cfgtmp[std::string((char *)namebuf)].t_str() + t_string((const char *)valbuf);
						}
						transcnt --;
					}
					
				} else {
					cfgtmp[std::string((char *)namebuf)] = t_string((const char *)valbuf);
				}
			}
		}
	}

	retval = 0;
exit:

	return retval;
}

#define MAXLEN_TEXTDOMAIN		63

void wml_config_from_file(const std::string &fname, config &cfg, uint32_t* nfiles, uint32_t* sum_size, uint32_t* modified)
{
	posix_file_t						fp = INVALID_FILE;
	uint32_t							datalen, fsizelow, fsizehigh, max_str_len, bytertd, tdcnt, idx, len;
	uint8_t								*data = NULL, *namebuf = NULL, *valbuf = NULL;
	char								tdname[MAXLEN_TEXTDOMAIN + 1];

	std::vector<std::string>			tdomain;

	posix_print("<xwml.cpp>::wml_config_from_file------fname: %s\n", fname.c_str());

	cfg.clear();	// 首先清空,以下是增加方式

	posix_fopen(fname.c_str(), GENERIC_READ, OPEN_EXISTING, fp);
	if (fp == INVALID_FILE) {
		posix_print("------<xwml.cpp>::wml_config_from_file, cannot create %s for read\n", fname.c_str());
		goto exit;
	}
	posix_fsize(fp, fsizelow, fsizehigh);
	if (fsizelow <= 16) {
		goto exit;
	}
	posix_fseek(fp, 0, 0);
	posix_fread(fp, &len, 4, bytertd);
	// 判断是不是XWML
	if (len != mmioFOURCC('X', 'W', 'M', 'L')) {
		goto exit;
	}
	posix_fread(fp, &len, 4, bytertd);
	if (nfiles) {
		*nfiles = len;
	}
	posix_fread(fp, &len, 4, bytertd);
	if (sum_size) {
		*sum_size = len;
	}
	posix_fread(fp, &len, 4, bytertd);
	if (modified) {
		*modified = len;
	}
	posix_fread(fp, &max_str_len, sizeof(max_str_len), bytertd);
			
	// 读出po
	posix_fread(fp, &tdcnt, sizeof(tdcnt), bytertd);
	datalen = fsizelow - 16 - sizeof(max_str_len) - sizeof(tdcnt);
	for (idx = 0; idx < tdcnt; idx ++) {
		posix_fread(fp, &len, sizeof(uint32_t), bytertd);
		posix_fread(fp, tdname, len, bytertd);
		tdname[len] = 0;
		tdomain.push_back(tdname);

		datalen -= sizeof(uint32_t) + len;

		t_string::add_textdomain(tdomain.back(), get_intl_dir());
	}
		
	data = (uint8_t *)malloc(datalen);
	namebuf = (uint8_t *)malloc(max_str_len + 1 + 1024);
	valbuf = (uint8_t *)malloc(max_str_len + 1 + 1024);

	// 剩下数据读出sram
	posix_fread(fp, data, datalen, bytertd);

	wml_config_from_data(data, datalen, namebuf, valbuf, tdomain, cfg);

exit:
	if (fp != INVALID_FILE) {
		posix_fclose(fp);
	}
	if (data) {
		free(data);
	}
	if (namebuf) {
		free(namebuf);
	}
	if (valbuf) {
		free(valbuf);
	}
	return;
}

bool wml_checksum_from_file(const std::string &fname, uint32_t* nfiles, uint32_t* sum_size, uint32_t* modified)
{
	posix_file_t fp = INVALID_FILE;
	uint32_t fsizelow, fsizehigh, bytertd, tmp;
	bool ok = false;

	posix_fopen(fname.c_str(), GENERIC_READ, OPEN_EXISTING, fp);
	if (fp == INVALID_FILE) {
		goto exit;
	}
	posix_fsize(fp, fsizelow, fsizehigh);
	if (fsizelow <= 16) {
		goto exit;
	}
	posix_fseek(fp, 0, 0);
	posix_fread(fp, &tmp, 4, bytertd);
	// 判断是不是XWML
	if (tmp != mmioFOURCC('X', 'W', 'M', 'L')) {
		goto exit;
	}
	posix_fread(fp, &tmp, 4, bytertd);
	if (nfiles) {
		*nfiles = tmp;
	}
	posix_fread(fp, &tmp, 4, bytertd);
	if (sum_size) {
		*sum_size = tmp;
	}
	posix_fread(fp, &tmp, 4, bytertd);
	if (modified) {
		*modified = tmp;
	}
	ok = true;		
exit:
	if (fp != INVALID_FILE) {
		posix_fclose(fp);
	}
	return ok;
}

void string_to_file(std::string str, char* fname)
{
	posix_file_t	fp;
	uint32_t		bytertd;

    posix_fopen(fname, GENERIC_WRITE, CREATE_ALWAYS, fp);
	if (fp == INVALID_FILE) {
		posix_print_mb("string_to_file, cann't create file %s for write", fname);
		return;
	}
	posix_fwrite(fp, str.c_str(), str.length(), bytertd);
	posix_fclose(fp);
	return;
}

unsigned char calcuate_xor_from_file(const std::string &fname)
{
	posix_file_t fp = INVALID_FILE;
	uint32_t fsizelow, fsizehigh, bytertd, pos;
	unsigned char ret = 0;
	unsigned char* data = NULL;

	posix_fopen(fname.c_str(), GENERIC_READ, OPEN_EXISTING, fp);
	if (fp == INVALID_FILE) {
		goto exit;
	}
	posix_fsize(fp, fsizelow, fsizehigh);
	if (!fsizelow && !fsizehigh) {
		goto exit;
	}
	posix_fseek(fp, 0, 0);
	data = (unsigned char*)malloc(fsizelow);
	
	posix_fread(fp, data, fsizelow, bytertd);
	pos = 0;
	while (pos < fsizelow) {
		ret ^= data[pos ++];
	}

exit:
	if (fp != INVALID_FILE) {
		posix_fclose(fp);
	}
	if (data) {
		free(data);
	}
	return ret;
}

/*
先执行parse_config，再执行wml_building_rules_from_file
425 ---> 12116: parse_config将近用了12秒
12116 ---> 29059: 改过之后将近用了17秒
===>在这种情况下wml_building_rules_from_file效率非但没提高反而是降低

先执行wml_building_rules_from_file，再执行parse_config，
wml_building_rules_from_file只是将近用了4秒
===>在这种情况下wml_building_rules_from_file效率明显提高

1、测试过应该不是wml_building_rules_from_file中building_rules_.clear问题。既使传下一个空的rules，wml_building_rules_from_file效率并没改变。
*/

// @size: 该值已经有效
#define t_list_to_fp(fp, list, idx, size, bytertd)	do {	\
	for (idx = 0; idx < size; idx ++) {	\
		posix_fwrite(fp, &(list)[idx].base, sizeof(t_translation::t_layer), bytertd);	\
		posix_fwrite(fp, &(list)[idx].overlay, sizeof(t_translation::t_layer), bytertd);	\
	}	\
} while (0)

//   2.1.如果该字符串是整窜第一串,它的hi8(hi16(u32))值是子串数目,否则置0
//   2.2.lo16(u32)是子串字符长度
#define vstr_to_fp(fp, vstr, idx, size, u32n, bytertd, max_str_len) do {	\
	size = (vstr).size();	\
	posix_fwrite(fp, &size, sizeof(size), bytertd);	\
	for (idx = 0; idx < size; idx ++) {	\
		u32n = (vstr)[idx].size();	\
		posix_fwrite(fp, &u32n, sizeof(u32n), bytertd);	\
		posix_fwrite(fp, (vstr)[idx].c_str(), u32n, bytertd);	\
		max_str_len = posix_max(max_str_len, u32n);	\
	}	\
} while (0)

void wml_building_rules_to_file(const std::string& fname, terrain_builder::building_rule* rules, uint32_t rules_size, uint32_t nfiles, uint32_t sum_size, uint32_t modified)
{
	posix_file_t						fp = INVALID_FILE;
	uint32_t							max_str_len, bytertd, u32n, idx, size, size1; 

	posix_print("<xwml.cpp>::wml_building_rules_to_file------fname: %s, will save %u rules\n", fname.c_str(), rules_size);

	posix_fopen(fname.c_str(), GENERIC_WRITE, CREATE_ALWAYS, fp);
	if (fp == INVALID_FILE) {
		posix_print("------<xwml.cpp>::wml_building_rules_to_file, cannot create %s for wrtie\n", fname.c_str());
		return;
	}

	// max str len
	max_str_len = 63;
	// 0--15
	u32n = mmioFOURCC('X', 'W', 'M', 'L');
	posix_fwrite(fp, &u32n, 4, bytertd);
	posix_fwrite(fp, &nfiles, 4, bytertd);
	posix_fwrite(fp, &sum_size, 4, bytertd);
	posix_fwrite(fp, &modified, 4, bytertd);

	posix_fseek(fp, 16 + sizeof(max_str_len), 0);
	// rules size
	posix_fwrite(fp, &rules_size, sizeof(rules_size), bytertd);

	uint32_t rule_index = 0;

	for (rule_index = 0; rule_index < rules_size; rule_index ++) {

		//
		// typedef std::multiset<building_rule> building_ruleset;
		//

		// building_rule
		const terrain_builder::building_rule& rule = rules[rule_index];

		// *int precedence
		posix_fwrite(fp, &rule.precedence, sizeof(int), bytertd);
		
		// *map_location location_constraints;
		posix_fwrite(fp, &rule.location_constraints.x, sizeof(int), bytertd);
		posix_fwrite(fp, &rule.location_constraints.y, sizeof(int), bytertd);

		// *int probability
		posix_fwrite(fp, &rule.probability, sizeof(int), bytertd);

		// size of constraints
		size = rule.constraints.size();
		posix_fwrite(fp, &size, sizeof(uint32_t), bytertd);

		// constraint_set constraints
		for (terrain_builder::constraint_set::const_iterator constraint = rule.constraints.begin(); constraint != rule.constraints.end(); ++constraint) {
			// typedef std::vector<terrain_constraint> constraint_set;
			posix_fwrite(fp, &constraint->loc.x, sizeof(int), bytertd);
			posix_fwrite(fp, &constraint->loc.y, sizeof(int), bytertd);

			//
			// terrain_constraint
			//

			// t_translation::t_match terrain_types_match;
			const t_translation::t_match& match = constraint->terrain_types_match;

//			struct t_match{
//				......
//				t_list terrain;
//				t_list mask;
//				t_list masked_terrain;
//				bool has_wildcard;
//				bool is_empty;
//			};

			// typedef std::vector<t_terrain> t_list;
			// terrain, mask和masked_terrain肯定一样长度, 为节省空间,只写一个长度值
			size = match.terrain.size();
			posix_fwrite(fp, &size, sizeof(size), bytertd);
			t_list_to_fp(fp, match.terrain, idx, size, bytertd);
			t_list_to_fp(fp, match.mask, idx, size, bytertd);
			t_list_to_fp(fp, match.masked_terrain, idx, size, bytertd);
			u32n = match.has_wildcard? 1: 0;
			posix_fwrite(fp, &u32n, sizeof(int), bytertd);
			u32n = match.is_empty? 1: 0;
			posix_fwrite(fp, &u32n, sizeof(int), bytertd);

			// std::vector<std::string> set_flag;
			vstr_to_fp(fp, constraint->set_flag, idx, size, u32n, bytertd, max_str_len);
			// std::vector<std::string> no_flag;
			vstr_to_fp(fp, constraint->no_flag, idx, size, u32n, bytertd, max_str_len);
			// std::vector<std::string> has_flag;
			vstr_to_fp(fp, constraint->has_flag, idx, size, u32n, bytertd, max_str_len);

			// (typedef std::vector<rule_image> rule_imagelist) rule_imagelist images
			size = constraint->images.size();
			posix_fwrite(fp, &size, sizeof(size), bytertd);
			for (idx = 0; idx < size; idx ++) {
				const struct terrain_builder::rule_image& ri = constraint->images[idx];
				// rule_image

//				struct rule_image {
//					......
//					int layer;
//					int basex, basey;
//					bool global_image;
//					int center_x, center_y;
//					rule_image_variantlist variants;
//				}

				posix_fwrite(fp, &ri.layer, sizeof(int), bytertd);
				posix_fwrite(fp, &ri.basex, sizeof(int), bytertd);
				posix_fwrite(fp, &ri.basey, sizeof(int), bytertd);
				u32n = ri.global_image? 1: 0;
				posix_fwrite(fp, &u32n, sizeof(int), bytertd);
				posix_fwrite(fp, &ri.center_x, sizeof(int), bytertd);
				posix_fwrite(fp, &ri.center_y, sizeof(int), bytertd);

				// (std::vector<rule_image_variant>) variants
				size1 = ri.variants.size();
				posix_fwrite(fp, &size1, sizeof(size), bytertd);
				for (std::vector<terrain_builder::rule_image_variant>::const_iterator imgitor = ri.variants.begin(); imgitor != ri.variants.end(); ++imgitor) {
					// value: rule_image_variant

//					struct rule_image_variant {
//						......
//						std::string image_string;
//						std::string variations;
//						std::string tod;
//						animated<image::locator> image;
//						bool random_start;
//					}

					u32n = imgitor->image_string.size();
					posix_fwrite(fp, &u32n, sizeof(u32n), bytertd);	
					posix_fwrite(fp, imgitor->image_string.c_str(), u32n, bytertd);
					max_str_len = posix_max(max_str_len, u32n);

					u32n = imgitor->variations.size();
					posix_fwrite(fp, &u32n, sizeof(u32n), bytertd);	
					posix_fwrite(fp, imgitor->variations.c_str(), u32n, bytertd);
					max_str_len = posix_max(max_str_len, u32n);

					u32n = imgitor->random_start? 1: 0;
					posix_fwrite(fp, &u32n, sizeof(int), bytertd);


					//
					// 到此为止，只有rule_image_variant中的image没有生成
					// image涉及到两个类class animated和class locator，类中有数据private成员数据，很难直接赋值
					// 考虑到from时也会调用构造函数以构造类，这里就填上构造函数需的参数就行了。
					// 如何由构造函数生成animated和locator参考：bool terrain_builder::start_animation(building_rule &rule)
					//

					// class animated
					// int starting_frame_time_;
					// bool does_not_change_;
					// bool started_;
					// bool need_first_update_;
					// int start_tick_;
					// bool cycles_;
					// double acceleration_;
					// int last_update_tick_;
					// int current_frame_key_;
					// +std::vector<frame> frames_;
					// int duration_;
					// int start_time_;
					// +T value_;
					// +class locator
					// static int last_index_;
					// int index_;
					// value val_;

//					struct value {
//						......
//						type type_;
//						std::string filename_;
//						map_location loc_;
//						std::string modifications_;
//						int center_x_;
//						int center_y_;
//					}

				}
			}
		}
	}

	// 更新最大的存储区大小
	posix_fseek(fp, 16, 0);
	posix_fwrite(fp, &max_str_len, sizeof(max_str_len), bytertd);

	posix_fclose(fp);

	posix_print("------<xwml.cpp>::wml_building_rules_to_file, return\n");
	return;
}

#define MAXLEN_BR_STRPLUS1		270

typedef struct {
	int first;
	int second;
} tmp_pair;

terrain_builder::building_rule* wml_building_rules_from_file(const std::string& fname, uint32_t* rules_size_ptr)
{
	posix_file_t fp = INVALID_FILE;
	uint32_t datalen, max_str_len, rules_size, fsizelow, fsizehigh, bytertd, idx, len, size, size1, idx1, size2, idx2;
	uint8_t* data = NULL, *strbuf = NULL, *variations = NULL;
	uint8_t* rdpos;
	terrain_builder::building_rule * rules = NULL;
	map_location loc;
	tmp_pair tmppair;

	posix_print("<xwml.cpp>::wml_config_from_file------fname: %s\n", fname.c_str());

	if (rules_size_ptr) {
		*rules_size_ptr = 0;
	}

	posix_fopen(fname.c_str(), GENERIC_READ, OPEN_EXISTING, fp);
	if (fp == INVALID_FILE) {
		posix_print("------<xwml.cpp>::wml_building_rules_from_file, cannot create %s for read\n", fname.c_str());
		return NULL;
	}
	posix_fsize(fp, fsizelow, fsizehigh);
	if (fsizelow <= 16 + sizeof(max_str_len) + sizeof(rules_size)) {
		posix_fclose(fp);
		return NULL;
	}
	posix_fseek(fp, 16, 0);
	posix_fread(fp, &max_str_len, sizeof(max_str_len), bytertd);
	posix_fread(fp, &rules_size, sizeof(rules_size), bytertd);

	datalen = fsizelow - 16 - sizeof(max_str_len) - sizeof(rules_size);
	data = (uint8_t *)malloc(datalen);
	strbuf = (uint8_t *)malloc(max_str_len + 1);
	variations = (uint8_t *)malloc(max_str_len + 1);

	// 剩下数据读出sram
	posix_fread(fp, data, datalen, bytertd);

	posix_print("max_str_len: %u, fsizelow: %u, datalen: %u\n", max_str_len, fsizelow, datalen);
	
	rdpos = data;

	// allocate memory for rules pointer array
	if (rules_size) {
		// rules = (terrain_builder::building_rule**)malloc(rules_size * sizeof(terrain_builder::building_rule**));
		rules = new terrain_builder::building_rule[rules_size];
	} else {
		rules = NULL;
	}

	uint32_t rule_index = 0;
/*
	uint32_t previous, current, start, stop = SDL_GetTicks();
*/
	while (rdpos < data + datalen) {
		int x, y;
/*
		start = stop;
		posix_print("#%04u, (", rule_index);
*/
		// building_ruleset::iterator 
		// terrain_builder::building_rule& pbr = *rules.insert(terrain_builder::building_rule());
		// rules.push_back(terrain_builder::building_rule());
		// terrain_builder::building_rule& pbr = rules.back();
		// rules[rule_index] = new terrain_builder::building_rule;
		terrain_builder::building_rule& pbr = rules[rule_index];
/*
		previous = SDL_GetTicks();
		posix_print("%u", previous - start);
*/

		// precedence
		memcpy(&pbr.precedence, rdpos, sizeof(int));
		rdpos = rdpos + sizeof(int);

		// *map_location location_constraints;
		memcpy(&x, rdpos, sizeof(int));
		memcpy(&y, rdpos + sizeof(int), sizeof(int));
		rdpos = rdpos + 2 * sizeof(int);
		pbr.location_constraints = map_location(x, y);

		// *int probability
		memcpy(&pbr.probability, rdpos, sizeof(int));
		rdpos = rdpos + sizeof(int);

		// local
		pbr.local = false;

		// size of constraints
		memcpy(&size, rdpos, sizeof(int));
		rdpos = rdpos + sizeof(int);
/*
		current = SDL_GetTicks();
		posix_print(" + %u", current - previous);
		previous = current;
*/
		for (idx = 0; idx < size; idx ++) {
			// terrain_constraint
			memcpy(&tmppair, rdpos, 8);
			rdpos = rdpos + 8;
			loc = map_location(tmppair.first, tmppair.second);

			// pbr.constraints[loc] = terrain_builder::terrain_constraint(loc);
			// constraint = pbr.constraints.find(loc);
			pbr.constraints.push_back(terrain_builder::terrain_constraint(loc));
			terrain_builder::terrain_constraint& constraint = pbr.constraints.back();

			//
			// t_mach
			//
			t_translation::t_match& match = constraint.terrain_types_match;

			// memcpy(&x, rdpos, sizeof(int));
			// memcpy(&y, rdpos, sizeof(int));
			// rdpos = rdpos + 2 * sizeof(int);

			// size of terrain in t_mach
			// constraints[loc].terrain_types_match = t_translation::t_terrain(x, y);

			memcpy(&size1, rdpos, sizeof(uint32_t));
			rdpos = rdpos + sizeof(uint32_t);
			for (idx1 = 0; idx1 < size1; idx1 ++) {
				memcpy(&tmppair, rdpos, 8);
				match.terrain.push_back(t_translation::t_terrain(tmppair.first, tmppair.second));
				rdpos = rdpos + 8;
			}
			for (idx1 = 0; idx1 < size1; idx1 ++) {
				memcpy(&tmppair, rdpos, 8);
				match.mask.push_back(t_translation::t_terrain(tmppair.first, tmppair.second));
				rdpos = rdpos + 8;
			}
			for (idx1 = 0; idx1 < size1; idx1 ++) {
				memcpy(&tmppair, rdpos, 8);
				match.masked_terrain.push_back(t_translation::t_terrain(tmppair.first, tmppair.second));
				rdpos = rdpos + 8;
			}
			memcpy(&tmppair, rdpos, 8);
			match.has_wildcard = tmppair.first? true: false;
			match.is_empty = tmppair.second? true: false;
			rdpos = rdpos + 8;

			// size of flags in set_flag
			memcpy(&size1, rdpos, sizeof(uint32_t));
			rdpos = rdpos + sizeof(uint32_t);
			for (idx1 = 0; idx1 < size1; idx1 ++) {
				memcpy(&len, rdpos, sizeof(uint32_t));
				memcpy(strbuf, rdpos + sizeof(uint32_t), len);
				strbuf[len] = 0;
				constraint.set_flag.push_back((char*)strbuf);
				rdpos = rdpos + sizeof(uint32_t) + len;				
			}
			// size of flags in no_flag
			memcpy(&size1, rdpos, sizeof(uint32_t));
			rdpos = rdpos + sizeof(uint32_t);
			for (idx1 = 0; idx1 < size1; idx1 ++) {
				memcpy(&len, rdpos, sizeof(uint32_t));
				memcpy(strbuf, rdpos + sizeof(uint32_t), len);
				strbuf[len] = 0;
				constraint.no_flag.push_back((char*)strbuf);
				rdpos = rdpos + sizeof(uint32_t) + len;				
			}
			// size of flags in has_flag
			memcpy(&size1, rdpos, sizeof(uint32_t));
			rdpos = rdpos + sizeof(uint32_t);
			for (idx1 = 0; idx1 < size1; idx1 ++) {
				memcpy(&len, rdpos, sizeof(uint32_t));
				memcpy(strbuf, rdpos + sizeof(uint32_t), len);
				strbuf[len] = 0;
				constraint.has_flag.push_back((char*)strbuf);
				rdpos = rdpos + sizeof(uint32_t) + len;				
			}

			// size of rule_image in rule_imagelist
			memcpy(&size1, rdpos, sizeof(uint32_t));
			rdpos = rdpos + sizeof(uint32_t);
			for (idx1 = 0; idx1 < size1; idx1 ++) {
				// struct terrain_builder::rule_image& ri = constraint->second.images[idx1];
				// rule_image
				int layer, center_x, center_y;
				bool global_image;

				memcpy(&layer, rdpos, sizeof(int));
				memcpy(&x, rdpos + 4, sizeof(int));
				memcpy(&y, rdpos + 8, sizeof(int));
				memcpy(&len, rdpos + 12, sizeof(int));
				global_image = len? true: false;
				memcpy(&center_x, rdpos + 16, sizeof(int));
				memcpy(&center_y, rdpos + 20, sizeof(int));
				rdpos = rdpos + 24;

				constraint.images.push_back(terrain_builder::rule_image(layer, x, y, global_image, center_x, center_y));

				// size of rule_image in rule_imagelist
				memcpy(&size2, rdpos, sizeof(uint32_t));
				rdpos = rdpos + sizeof(uint32_t);
				for (idx2 = 0; idx2 < size2; idx2 ++) {
					bool random_start;

					memcpy(&len, rdpos, sizeof(uint32_t));
					memcpy(strbuf, rdpos + sizeof(uint32_t), len);
					strbuf[len] = 0;
					rdpos = rdpos + sizeof(uint32_t) + len;

					// Adds the main (default) variant of the image, if present
					memcpy(&len, rdpos, sizeof(uint32_t));
					memcpy(variations, rdpos + sizeof(uint32_t), len);
					variations[len] = 0;
					rdpos = rdpos + sizeof(uint32_t) + len;

					memcpy(&len, rdpos, sizeof(int));
					random_start = len? true: false;
					rdpos = rdpos + sizeof(int);
					constraint.images.back().variants.push_back(terrain_builder::rule_image_variant((char*)strbuf, (char*)variations, random_start));
				}
			}
/*
			current = SDL_GetTicks();
			posix_print(" + %u", current - previous);
			previous = current;
*/
		}
/*
		stop = SDL_GetTicks();
		posix_print("), expend %u ms\n", stop - start);
*/
		rule_index ++;
	}

	if (fp != INVALID_FILE) {
		posix_fclose(fp);
	}
	if (data) {
		free(data);
	}
	if (strbuf) {
		free(strbuf);
	}
	if (variations) {
		free(variations);
	}

	if (rules_size_ptr) {
		*rules_size_ptr = rule_index;
	}

	posix_print("------<xwml.cpp>::wml_building_rules_from_file, restore %u rules, return\n", rule_index);
	return rules;
}
