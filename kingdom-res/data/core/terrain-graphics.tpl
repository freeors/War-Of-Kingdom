[macro]
	type="NONE"
	[cfg]
		map="
    , 2
    2 , 2
    , 1
    2 , 2
    , 2"
		[tile]
			pos=1
			set_no_flag="fakemapedge"
			type="*^_fme"
		[/tile]
		[tile]
			pos=2
			type="*^_fme"
		[/tile]
		[image]
			center="90,144"
			layer=1000
			name="off-map/border.png"
		[/image]
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="(!,*^_fme)"
		1_adjacent="(*^_fme)"
		2_layer=1000
		3_imagestem="off-map/border"
	[/cfg]
[/macro]

[macro]
	type="EDITOR_OVERLAY"
	[cfg]
		0_type="*^Xo"
		1_image="impassable-editor"
	[/cfg]
[/macro]

[macro]
	type="EDITOR_OVERLAY"
	[cfg]
		0_type="*^Qov"
		1_image="unwalkable-editor"
	[/cfg]
[/macro]

[macro]
	type="EDITOR_OVERLAY"
	[cfg]
		0_type="*^Vov"
		1_image="village/village-overlay-editor"
	[/cfg]
[/macro]

[macro]
	type="EDITOR_OVERLAY"
	[cfg]
		0_type="*^Cov"
		1_image="castle/castle-overlay-editor"
	[/cfg]
[/macro]

[macro]
	type="EDITOR_OVERLAY"
	[cfg]
		0_type="*^Kov"
		1_image="castle/keep-overlay-editor"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:STRAIGHTS"
	[cfg]
		0_nw_se_overlay="Bsb\"
		1_n_s_overlay="Bsb|"
		2_ne_sw_overlay="Bsb/"
		3_bn_terrain="*"
		4_bs_terrain="*"
		5_s_terrain="*"
		6_name="stonebridge"
		7_layer=-80
		8_image_group="bridge/stonebridge"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:ENDS"
	[cfg]
		0_nw_se_overlay="Bsb\"
		1_n_s_overlay="Bsb|"
		2_ne_sw_overlay="Bsb/"
		3_b_terrain="*"
		4_e_terrain="(Co*,Cu*,Ko*,Ku*)"
		5_s_terrain="*"
		6_name="stonebridge"
		7_layer=-80
		8_image_group="bridge/stonebridge-castle"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:ENDS"
	[cfg]
		0_nw_se_overlay="Bsb\"
		1_n_s_overlay="Bsb|"
		2_ne_sw_overlay="Bsb/"
		3_b_terrain="*"
		4_e_terrain="(C*,K*)"
		5_s_terrain="*"
		6_name="stonebridge"
		7_layer=-80
		8_image_group="bridge/stonebridge-castle"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:ENDS"
	[cfg]
		0_nw_se_overlay="Bsb\"
		1_n_s_overlay="Bsb|"
		2_ne_sw_overlay="Bsb/"
		3_b_terrain="*"
		4_e_terrain="(C*,K*)"
		5_s_terrain="*"
		6_name="stonebridge"
		7_layer=-80
		8_image_group="bridge/stonebridge-short"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:ENDS"
	[cfg]
		0_nw_se_overlay="Bsb\"
		1_n_s_overlay="Bsb|"
		2_ne_sw_overlay="Bsb/"
		3_b_terrain="*"
		4_e_terrain="(W*,S*)"
		5_s_terrain="*"
		6_name="stonebridge"
		7_layer=-80
		8_image_group="bridge/stonebridge-water"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:ENDS"
	[cfg]
		0_nw_se_overlay="Bsb\"
		1_n_s_overlay="Bsb|"
		2_ne_sw_overlay="Bsb/"
		3_b_terrain="*"
		4_e_terrain="*"
		5_s_terrain="*"
		6_name="stonebridge"
		7_layer=-80
		8_image_group="bridge/stonebridge"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:STRAIGHTS"
	[cfg]
		0_nw_se_overlay="Bh\"
		1_n_s_overlay="Bh|"
		2_ne_sw_overlay="Bh/"
		3_bn_terrain="Q*"
		4_bs_terrain="Q*"
		5_s_terrain="*"
		6_name="hanging"
		7_layer=-80
		8_image_group="bridge/hanging-xx"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:STRAIGHTS"
	[cfg]
		0_nw_se_overlay="Bh\"
		1_n_s_overlay="Bh|"
		2_ne_sw_overlay="Bh/"
		3_bn_terrain="Q*"
		4_bs_terrain="*"
		5_s_terrain="*"
		6_name="hanging"
		7_layer=-80
		8_image_group="bridge/hanging-x"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:STRAIGHTS"
	[cfg]
		0_nw_se_overlay="Bh\"
		1_n_s_overlay="Bh|"
		2_ne_sw_overlay="Bh/"
		3_bn_terrain="*"
		4_bs_terrain="*"
		5_s_terrain="*"
		6_name="hanging"
		7_layer=-80
		8_image_group="bridge/hanging"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:ENDS"
	[cfg]
		0_nw_se_overlay="Bh\"
		1_n_s_overlay="Bh|"
		2_ne_sw_overlay="Bh/"
		3_b_terrain="Q*"
		4_e_terrain="(W*,S*)"
		5_s_terrain="*"
		6_name="hanging"
		7_layer=-80
		8_image_group="bridge/hanging-wx"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:ENDS"
	[cfg]
		0_nw_se_overlay="Bh\"
		1_n_s_overlay="Bh|"
		2_ne_sw_overlay="Bh/"
		3_b_terrain="Q*"
		4_e_terrain="(C*,K*)"
		5_s_terrain="*"
		6_name="hanging"
		7_layer=-80
		8_image_group="bridge/hanging-cx"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:ENDS"
	[cfg]
		0_nw_se_overlay="Bh\"
		1_n_s_overlay="Bh|"
		2_ne_sw_overlay="Bh/"
		3_b_terrain="Q*"
		4_e_terrain="*"
		5_s_terrain="*"
		6_name="hanging"
		7_layer=-80
		8_image_group="bridge/hanging-x"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:ENDS"
	[cfg]
		0_nw_se_overlay="Bh\"
		1_n_s_overlay="Bh|"
		2_ne_sw_overlay="Bh/"
		3_b_terrain="*"
		4_e_terrain="(W*,S*)"
		5_s_terrain="*"
		6_name="hanging"
		7_layer=-80
		8_image_group="bridge/hanging-w"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:ENDS"
	[cfg]
		0_nw_se_overlay="Bh\"
		1_n_s_overlay="Bh|"
		2_ne_sw_overlay="Bh/"
		3_b_terrain="*"
		4_e_terrain="(C*,K*)"
		5_s_terrain="*"
		6_name="hanging"
		7_layer=-80
		8_image_group="bridge/hanging-c"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:ENDS"
	[cfg]
		0_nw_se_overlay="Bh\"
		1_n_s_overlay="Bh|"
		2_ne_sw_overlay="Bh/"
		3_b_terrain="*"
		4_e_terrain="*"
		5_s_terrain="*"
		6_name="hanging"
		7_layer=-80
		8_image_group="bridge/hanging"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:STRAIGHTS"
	[cfg]
		0_nw_se_overlay="Bcx\"
		1_n_s_overlay="Bcx|"
		2_ne_sw_overlay="Bcx/"
		3_bn_terrain="*"
		4_bs_terrain="Ql*"
		5_s_terrain="Ql*"
		6_name="chasm"
		7_layer=-80
		8_image_group="bridge/chasm-ll"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:STRAIGHTS"
	[cfg]
		0_nw_se_overlay="Bcx\"
		1_n_s_overlay="Bcx|"
		2_ne_sw_overlay="Bcx/"
		3_bn_terrain="*"
		4_bs_terrain="Ql*"
		5_s_terrain="Q*"
		6_name="chasm"
		7_layer=-80
		8_image_group="bridge/chasm-lx"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:STRAIGHTS"
	[cfg]
		0_nw_se_overlay="Bcx\"
		1_n_s_overlay="Bcx|"
		2_ne_sw_overlay="Bcx/"
		3_bn_terrain="*"
		4_bs_terrain="Q*"
		5_s_terrain="Ql*"
		6_name="chasm"
		7_layer=-80
		8_image_group="bridge/chasm-xl"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:STRAIGHTS"
	[cfg]
		0_nw_se_overlay="Bcx\"
		1_n_s_overlay="Bcx|"
		2_ne_sw_overlay="Bcx/"
		3_bn_terrain="*"
		4_bs_terrain="Q*"
		5_s_terrain="Q*"
		6_name="chasm"
		7_layer=-80
		8_image_group="bridge/chasm-xx"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:STRAIGHTS"
	[cfg]
		0_nw_se_overlay="Bcx\"
		1_n_s_overlay="Bcx|"
		2_ne_sw_overlay="Bcx/"
		3_bn_terrain="*"
		4_bs_terrain="*"
		5_s_terrain="*"
		6_name="chasm"
		7_layer=-80
		8_image_group="bridge/chasm"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:ENDS"
	[cfg]
		0_nw_se_overlay="Bcx\"
		1_n_s_overlay="Bcx|"
		2_ne_sw_overlay="Bcx/"
		3_b_terrain="Ql*"
		4_e_terrain="Ql*"
		5_s_terrain="Ql*"
		6_name="chasm"
		7_layer=-80
		8_image_group="bridge/chasm-dock-ll"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:ENDS"
	[cfg]
		0_nw_se_overlay="Bcx\"
		1_n_s_overlay="Bcx|"
		2_ne_sw_overlay="Bcx/"
		3_b_terrain="Ql*"
		4_e_terrain="Ql*"
		5_s_terrain="Q*"
		6_name="chasm"
		7_layer=-80
		8_image_group="bridge/chasm-dock-lx"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:ENDS"
	[cfg]
		0_nw_se_overlay="Bcx\"
		1_n_s_overlay="Bcx|"
		2_ne_sw_overlay="Bcx/"
		3_b_terrain="Qx*"
		4_e_terrain="Qx*"
		5_s_terrain="Ql*"
		6_name="chasm"
		7_layer=-80
		8_image_group="bridge/chasm-dock-xl"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:ENDS"
	[cfg]
		0_nw_se_overlay="Bcx\"
		1_n_s_overlay="Bcx|"
		2_ne_sw_overlay="Bcx/"
		3_b_terrain="Qx*"
		4_e_terrain="Qx*"
		5_s_terrain="Qx*"
		6_name="chasm"
		7_layer=-80
		8_image_group="bridge/chasm-dock-xx"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:ENDS"
	[cfg]
		0_nw_se_overlay="Bcx\"
		1_n_s_overlay="Bcx|"
		2_ne_sw_overlay="Bcx/"
		3_b_terrain="Ql*"
		4_e_terrain="Q*"
		5_s_terrain="Ql*"
		6_name="chasm"
		7_layer=-80
		8_image_group="bridge/chasm-dock-ll"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:ENDS"
	[cfg]
		0_nw_se_overlay="Bcx\"
		1_n_s_overlay="Bcx|"
		2_ne_sw_overlay="Bcx/"
		3_b_terrain="Ql*"
		4_e_terrain="Q*"
		5_s_terrain="Q*"
		6_name="chasm"
		7_layer=-80
		8_image_group="bridge/chasm-dock-lx"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:ENDS"
	[cfg]
		0_nw_se_overlay="Bcx\"
		1_n_s_overlay="Bcx|"
		2_ne_sw_overlay="Bcx/"
		3_b_terrain="Q*"
		4_e_terrain="Q*"
		5_s_terrain="Ql*"
		6_name="chasm"
		7_layer=-80
		8_image_group="bridge/chasm-dock-xl"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:ENDS"
	[cfg]
		0_nw_se_overlay="Bcx\"
		1_n_s_overlay="Bcx|"
		2_ne_sw_overlay="Bcx/"
		3_b_terrain="Q*"
		4_e_terrain="Q*"
		5_s_terrain="Q*"
		6_name="chasm"
		7_layer=-80
		8_image_group="bridge/chasm-dock-xx"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:ENDS"
	[cfg]
		0_nw_se_overlay="Bcx\"
		1_n_s_overlay="Bcx|"
		2_ne_sw_overlay="Bcx/"
		3_b_terrain="*"
		4_e_terrain="Ql*"
		5_s_terrain="*"
		6_name="chasm"
		7_layer=-80
		8_image_group="bridge/chasm-dock-l"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:ENDS"
	[cfg]
		0_nw_se_overlay="Bcx\"
		1_n_s_overlay="Bcx|"
		2_ne_sw_overlay="Bcx/"
		3_b_terrain="*"
		4_e_terrain="Q*"
		5_s_terrain="*"
		6_name="chasm"
		7_layer=-80
		8_image_group="bridge/chasm-dock"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:ENDS"
	[cfg]
		0_nw_se_overlay="Bcx\"
		1_n_s_overlay="Bcx|"
		2_ne_sw_overlay="Bcx/"
		3_b_terrain="*"
		4_e_terrain="*"
		5_s_terrain="Qx*"
		6_name="chasm"
		7_layer=-80
		8_image_group="bridge/chasm-x"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:ENDS"
	[cfg]
		0_nw_se_overlay="Bcx\"
		1_n_s_overlay="Bcx|"
		2_ne_sw_overlay="Bcx/"
		3_b_terrain="*"
		4_e_terrain="*"
		5_s_terrain="Ql*"
		6_name="chasm"
		7_layer=-80
		8_image_group="bridge/chasm-l"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:ENDS"
	[cfg]
		0_nw_se_overlay="Bcx\"
		1_n_s_overlay="Bcx|"
		2_ne_sw_overlay="Bcx/"
		3_b_terrain="*"
		4_e_terrain="*"
		5_s_terrain="*"
		6_name="chasm"
		7_layer=-80
		8_image_group="bridge/chasm"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:JOINTS"
	[cfg]
		0_nw_se_overlay="Bp\"
		1_n_s_overlay="Bp|"
		2_ne_sw_overlay="Bp/"
		3_b_terrain="*"
		4_s_terrain="Q*"
		5_name="planks"
		6_layer=-80
		7_image_group="bridge/planks"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:JOINTS"
	[cfg]
		0_nw_se_overlay="Bp\"
		1_n_s_overlay="Bp|"
		2_ne_sw_overlay="Bp/"
		3_b_terrain="*"
		4_s_terrain="*"
		5_name="planks"
		6_layer=-80
		7_image_group="bridge/planks-short"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:CORNERS"
	[cfg]
		0_nw_se_overlay="Bp\"
		1_n_s_overlay="Bp|"
		2_ne_sw_overlay="Bp/"
		3_b_terrain="*"
		4_s_terrain="Q*"
		5_name="planks"
		6_layer=-80
		7_image_group="bridge/planks"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:CORNERS"
	[cfg]
		0_nw_se_overlay="Bp\"
		1_n_s_overlay="Bp|"
		2_ne_sw_overlay="Bp/"
		3_b_terrain="*"
		4_s_terrain="*"
		5_name="planks"
		6_layer=-80
		7_image_group="bridge/planks-short"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:STRAIGHTS"
	[cfg]
		0_nw_se_overlay="Bp\"
		1_n_s_overlay="Bp|"
		2_ne_sw_overlay="Bp/"
		3_bn_terrain="*"
		4_bs_terrain="*"
		5_s_terrain="Q*"
		6_name="planks"
		7_layer=-80
		8_image_group="bridge/planks"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:STRAIGHTS"
	[cfg]
		0_nw_se_overlay="Bp\"
		1_n_s_overlay="Bp|"
		2_ne_sw_overlay="Bp/"
		3_bn_terrain="*"
		4_bs_terrain="*"
		5_s_terrain="*"
		6_name="planks"
		7_layer=-80
		8_image_group="bridge/planks-short"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:ENDS"
	[cfg]
		0_nw_se_overlay="Bp\"
		1_n_s_overlay="Bp|"
		2_ne_sw_overlay="Bp/"
		3_b_terrain="*"
		4_e_terrain="Q*"
		5_s_terrain="Q*"
		6_name="planks"
		7_layer=-80
		8_image_group="bridge/planks-dock"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:ENDS"
	[cfg]
		0_nw_se_overlay="Bp\"
		1_n_s_overlay="Bp|"
		2_ne_sw_overlay="Bp/"
		3_b_terrain="*"
		4_e_terrain="Q*"
		5_s_terrain="*"
		6_name="planks"
		7_layer=-80
		8_image_group="bridge/planks-short-dock"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:ENDS"
	[cfg]
		0_nw_se_overlay="Bp\"
		1_n_s_overlay="Bp|"
		2_ne_sw_overlay="Bp/"
		3_b_terrain="*"
		4_e_terrain="*"
		5_s_terrain="Q*"
		6_name="planks"
		7_layer=-80
		8_image_group="bridge/planks"
	[/cfg]
[/macro]

[macro]
	type="BRIDGE:ENDS"
	[cfg]
		0_nw_se_overlay="Bp\"
		1_n_s_overlay="Bp|"
		2_ne_sw_overlay="Bp/"
		3_b_terrain="*"
		4_e_terrain="*"
		5_s_terrain="*"
		6_name="planks"
		7_layer=-80
		8_image_group="bridge/planks-short"
	[/cfg]
[/macro]

[macro]
	type="LAYOUT_TRACKS_F"
	[cfg]
		0_se_nw_value="*^Bw\*"
		1_n_s_value="*^Bw|*"
		2_ne_sw_value="*^Bw/*"
		3_flag="track_wood"
	[/cfg]
[/macro]

[macro]
	type="LAYOUT_TRACKS_F"
	[cfg]
		0_se_nw_value="*^Br\"
		1_n_s_value="*^Br|"
		2_ne_sw_value="*^Br/"
		3_flag="track_rail"
	[/cfg]
[/macro]

[macro]
	type="TRACK_COMPLETE"
	[cfg]
		0_se_nw_value="*^Br\"
		1_n_s_value="*^Br|"
		2_ne_sw_value="*^Br/"
		3_flag="track_rail"
		4_imagestem="misc/rails"
	[/cfg]
[/macro]

[macro]
	type="TRACK_BORDER_RESTRICTED_PLF"
	[cfg]
		0_terrainlist="(*^Br/,*^Br\)"
		1_adjacent="*^Br|"
		2_prob=100
		3_layer=-80
		4_flag="track_rail"
		5_imagestem="misc/rails-switch-ns"
	[/cfg]
[/macro]

[macro]
	type="TRACK_BORDER_RESTRICTED_PLF"
	[cfg]
		0_terrainlist="(*^Br|,*^Br/)"
		1_adjacent="*^Br\"
		2_prob=100
		3_layer=-80
		4_flag="track_rail"
		5_imagestem="misc/rails-switch-nwse"
	[/cfg]
[/macro]

[macro]
	type="TRACK_BORDER_RESTRICTED_PLF"
	[cfg]
		0_terrainlist="(*^Br|,*^Br\)"
		1_adjacent="*^Br/"
		2_prob=100
		3_layer=-80
		4_flag="track_rail"
		5_imagestem="misc/rails-switch-nesw"
	[/cfg]
[/macro]

[macro]
	type="TRACK_BORDER_RESTRICTED_PLF"
	[cfg]
		0_terrainlist="(*^Br|,*^Br/,*^Br\)"
		1_adjacent="(!,C*,K*)"
		2_prob=100
		3_layer=-80
		4_flag="track_rail"
		5_imagestem="misc/rails-end"
	[/cfg]
[/macro]

[macro]
	type="TRACK_COMPLETE"
	[cfg]
		0_se_nw_value="*^Bw\"
		1_n_s_value="*^Bw|"
		2_ne_sw_value="*^Bw/"
		3_flag="track_wood"
		4_imagestem="bridge/wood"
	[/cfg]
[/macro]

[macro]
	type="TRACK_COMPLETE"
	[cfg]
		0_se_nw_value="*^Bw\r"
		1_n_s_value="*^Bw|r"
		2_ne_sw_value="*^Bw/r"
		3_flag="track_wood"
		4_imagestem="bridge/wood-rotting"
	[/cfg]
[/macro]

[macro]
	type="TRACK_BORDER_RESTRICTED_PLF"
	[cfg]
		0_terrainlist="(*^Bw|*,*^Bw/*,*^Bw\*)"
		1_adjacent="(*^Bw|*,*^Bw/*,*^Bw/*)"
		2_prob=100
		3_layer=-80
		4_flag="track_wood"
		5_imagestem="bridge/wood-end"
	[/cfg]
[/macro]

[macro]
	type="TRACK_BORDER_RESTRICTED_PLF"
	[cfg]
		0_terrainlist="(*^Bw|*,*^Bw/*,*^Bw\*)"
		1_adjacent="(W*^,Ss^,Ai^)"
		2_prob=100
		3_layer=-80
		4_flag="track_wood"
		5_imagestem="bridge/wood-dock"
	[/cfg]
[/macro]

[macro]
	type="TRACK_BORDER_RESTRICTED_PLF"
	[cfg]
		0_terrainlist="(*^Bw|*,*^Bw/*,*^Bw\*)"
		1_adjacent="(!,C*,K*)"
		2_prob=100
		3_layer=-80
		4_flag="track_wood"
		5_imagestem="bridge/wood-end"
	[/cfg]
[/macro]

[macro]
	type="NEW:FOREST"
	[cfg]
		0_terrainlist="H*^Fp,M*^Fp"
		1_adjacent="(C*,K*,X*,Q*,W*,Ai,M*,*^V*,*^B*)"
		2_imagestem="forest/pine-sparse"
	[/cfg]
[/macro]

[macro]
	type="NEW:FOREST"
	[cfg]
		0_terrainlist="*^Fp"
		1_adjacent="(C*,K*,X*,Q*,W*,Ai,M*,*^V*,*^B*)"
		2_imagestem="forest/pine"
	[/cfg]
[/macro]

[macro]
	type="NEW:FOREST"
	[cfg]
		0_terrainlist="H*^Fpa,M*^Fpa"
		1_adjacent="(C*,K*,X*,Q*,W*,Ai,M*,*^V*,*^B*)"
		2_imagestem="forest/snow-forest-sparse"
	[/cfg]
[/macro]

[macro]
	type="NEW:FOREST"
	[cfg]
		0_terrainlist="*^Fpa"
		1_adjacent="(C*,K*,X*,Q*,W*,Ai,M*,*^V*,*^B*)"
		2_imagestem="forest/snow-forest"
	[/cfg]
[/macro]

[macro]
	type="NEW:FOREST"
	[cfg]
		0_terrainlist="H*^Ft,M*^Ft"
		1_adjacent="(C*,K*,X*,Q*,W*,Ai,M*,*^V*,*^B*)"
		2_imagestem="forest/tropical/jungle-sparse"
	[/cfg]
[/macro]

[macro]
	type="NEW:FOREST"
	[cfg]
		0_terrainlist="*^Ft"
		1_adjacent="(C*,K*,X*,Q*,W*,Ai,M*,*^V*,*^B*)"
		2_imagestem="forest/tropical/jungle"
	[/cfg]
[/macro]

[macro]
	type="NEW:FOREST"
	[cfg]
		0_terrainlist="H*^Ftd,M*^Ftd"
		1_adjacent="(C*,K*,X*,Q*,W*,Ai,M*,*^V*,*^B*)"
		2_imagestem="forest/tropical/palm-desert-sparse"
	[/cfg]
[/macro]

[macro]
	type="NEW:FOREST"
	[cfg]
		0_terrainlist="*^Ftd"
		1_adjacent="(C*,K*,X*,Q*,W*,Ai,M*,*^V*,*^B*)"
		2_imagestem="forest/tropical/palm-desert"
	[/cfg]
[/macro]

[macro]
	type="NEW:FOREST"
	[cfg]
		0_terrainlist="H*^Ftp,M*^Ftp"
		1_adjacent="(C*,K*,X*,Q*,W*,Ai,M*,*^V*,*^B*)"
		2_imagestem="forest/tropical/palms-sparse"
	[/cfg]
[/macro]

[macro]
	type="NEW:FOREST"
	[cfg]
		0_terrainlist="*^Ftp"
		1_adjacent="(C*,K*,X*,Q*,W*,Ai,M*,*^V*,*^B*)"
		2_imagestem="forest/tropical/palms"
	[/cfg]
[/macro]

[macro]
	type="NEW:FOREST"
	[cfg]
		0_terrainlist="*^Ftr"
		1_adjacent="(C*,K*,X*,Q*,W*,Ai,M*,*^V*,*^B*)"
		2_imagestem="forest/tropical/rainforest"
	[/cfg]
[/macro]

[macro]
	type="NEW:FOREST"
	[cfg]
		0_terrainlist="H*^Fts,M*^Fts"
		1_adjacent="(C*,K*,X*,Q*,W*,Ai,M*,*^V*,*^B*)"
		2_imagestem="forest/tropical/savanna-sparse"
	[/cfg]
[/macro]

[macro]
	type="NEW:FOREST"
	[cfg]
		0_terrainlist="*^Fts"
		1_adjacent="(C*,K*,X*,Q*,W*,Ai,M*,*^V*,*^B*)"
		2_imagestem="forest/tropical/savanna"
	[/cfg]
[/macro]

[macro]
	type="NEW:FOREST"
	[cfg]
		0_terrainlist="H*^Fds,M*^Fds"
		1_adjacent="(C*,K*,X*,Q*,W*,Ai,M*,*^V*,*^B*)"
		2_imagestem="forest/deciduous-summer-sparse"
	[/cfg]
[/macro]

[macro]
	type="NEW:FOREST"
	[cfg]
		0_terrainlist="*^Fds"
		1_adjacent="(C*,K*,X*,Q*,W*,Ai,M*,*^V*,*^B*)"
		2_imagestem="forest/deciduous-summer"
	[/cfg]
[/macro]

[macro]
	type="NEW:FOREST"
	[cfg]
		0_terrainlist="H*^Fdf,M*^Fdf"
		1_adjacent="(C*,K*,X*,Q*,W*,Ai,M*,*^V*,*^B*)"
		2_imagestem="forest/deciduous-fall-sparse"
	[/cfg]
[/macro]

[macro]
	type="NEW:FOREST"
	[cfg]
		0_terrainlist="*^Fdf"
		1_adjacent="(C*,K*,X*,Q*,W*,Ai,M*,*^V*,*^B*)"
		2_imagestem="forest/deciduous-fall"
	[/cfg]
[/macro]

[macro]
	type="NEW:FOREST"
	[cfg]
		0_terrainlist="H*^Fdw,M*^Fdw"
		1_adjacent="(C*,K*,X*,Q*,W*,Ai,M*,*^V*,*^B*)"
		2_imagestem="forest/deciduous-winter-sparse"
	[/cfg]
[/macro]

[macro]
	type="NEW:FOREST"
	[cfg]
		0_terrainlist="*^Fdw"
		1_adjacent="(C*,K*,X*,Q*,W*,Ai,M*,*^V*,*^B*)"
		2_imagestem="forest/deciduous-winter"
	[/cfg]
[/macro]

[macro]
	type="NEW:FOREST"
	[cfg]
		0_terrainlist="H*^Fda,M*^Fda"
		1_adjacent="(C*,K*,X*,Q*,W*,Ai,M*,*^V*,*^B*)"
		2_imagestem="forest/deciduous-winter-snow-sparse"
	[/cfg]
[/macro]

[macro]
	type="NEW:FOREST"
	[cfg]
		0_terrainlist="*^Fda"
		1_adjacent="(C*,K*,X*,Q*,W*,Ai,M*,*^V*,*^B*)"
		2_imagestem="forest/deciduous-winter-snow"
	[/cfg]
[/macro]

[macro]
	type="NEW:FOREST"
	[cfg]
		0_terrainlist="H*^Fms,M*^Fms"
		1_adjacent="(C*,K*,X*,Q*,W*,Ai,M*,*^V*,*^B*)"
		2_imagestem="forest/mixed-summer-sparse"
	[/cfg]
[/macro]

[macro]
	type="NEW:FOREST"
	[cfg]
		0_terrainlist="*^Fms"
		1_adjacent="(C*,K*,X*,Q*,W*,Ai,M*,*^V*,*^B*)"
		2_imagestem="forest/mixed-summer"
	[/cfg]
[/macro]

[macro]
	type="NEW:FOREST"
	[cfg]
		0_terrainlist="H*^Fmf,M*^Fmf"
		1_adjacent="(C*,K*,X*,Q*,W*,Ai,M*,*^V*,*^B*)"
		2_imagestem="forest/mixed-fall-sparse"
	[/cfg]
[/macro]

[macro]
	type="NEW:FOREST"
	[cfg]
		0_terrainlist="*^Fmf"
		1_adjacent="(C*,K*,X*,Q*,W*,Ai,M*,*^V*,*^B*)"
		2_imagestem="forest/mixed-fall"
	[/cfg]
[/macro]

[macro]
	type="NEW:FOREST"
	[cfg]
		0_terrainlist="H*^Fmw,M*^Fmw"
		1_adjacent="(C*,K*,X*,Q*,W*,Ai,M*,*^V*,*^B*)"
		2_imagestem="forest/mixed-winter-sparse"
	[/cfg]
[/macro]

[macro]
	type="NEW:FOREST"
	[cfg]
		0_terrainlist="*^Fmw"
		1_adjacent="(C*,K*,X*,Q*,W*,Ai,M*,*^V*,*^B*)"
		2_imagestem="forest/mixed-winter"
	[/cfg]
[/macro]

[macro]
	type="NEW:FOREST"
	[cfg]
		0_terrainlist="H*^Fma,M*^Fma"
		1_adjacent="(C*,K*,X*,Q*,W*,Ai,M*,*^V*,*^B*)"
		2_imagestem="forest/mixed-winter-snow-sparse"
	[/cfg]
[/macro]

[macro]
	type="NEW:FOREST"
	[cfg]
		0_terrainlist="*^Fma"
		1_adjacent="(C*,K*,X*,Q*,W*,Ai,M*,*^V*,*^B*)"
		2_imagestem="forest/mixed-winter-snow"
	[/cfg]
[/macro]

[macro]
	type="NEW:FOREST"
	[cfg]
		0_terrainlist="*^Uf*"
		1_adjacent="(C*,K*,X*,Q*,W*,Ai,M*,*^V*,*^B*)"
		2_imagestem="forest/mushrooms"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_RANDOM"
	[cfg]
		0_terrainlist="*^Fet"
		1_imagestem="forest/great-tree"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_RANDOM"
	[cfg]
		0_terrainlist="*^Fetd"
		1_imagestem="forest/great-tree-dead"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_P"
	[cfg]
		0_terrainlist="*^Do"
		1_prob=30
		2_imagestem="village/desert-oasis-1"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_P"
	[cfg]
		0_terrainlist="*^Do"
		1_prob=43
		2_imagestem="village/desert-oasis-2"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_P"
	[cfg]
		0_terrainlist="*^Do"
		1_prob=100
		2_imagestem="village/desert-oasis-3"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_COMPLETE_LF"
	[cfg]
		0_terrainlist="Ss"
		1_adjacent="(!,Xv,Chs,!,C*,H*,M*,X*,Q*,A*,Ke,Kea,Kud)"
		2_layer=-85
		3_flag="base2"
		4_imagestem="swamp/reed"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_COMPLETE_LF"
	[cfg]
		0_terrainlist="Chs"
		1_adjacent="(!,C*,K*,Ss)"
		2_layer=-85
		3_flag="base2"
		4_imagestem="swamp/reed"
	[/cfg]
[/macro]

[macro]
	type="VOLCANO_2x2"
	[cfg]
		0_terrain="Mv"
		1_adjacent="Mm,Md"
		2_prob=100
		3_flag="base2"
		4_imagestem="mountains/volcano6"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_RESTRICTED3_F"
	[cfg]
		0_terrain="Mm"
		1_adjacent="(!,Xv,!,C*,K*,X*,Ql,Qx*,W*)"
		2_flag="base2"
		3_imagestem="mountains/basic-castle-n"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_ROTATION_RESTRICTED2_F"
	[cfg]
		0_terrain="Mm"
		1_adjacent="(!,Xv,!,C*,K*,X*,Ql,Qx*,W*)"
		2_flag="base2"
		3_imagestem="mountains/basic-castle"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_RESTRICTED2_F"
	[cfg]
		0_terrain="Mm"
		1_adjacent="(!,Xv,!,C*,K*,X*,Ql,Qx*)"
		2_flag="base2"
		3_imagestem="mountains/basic-castle-n"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_ROTATION_RESTRICTED_F"
	[cfg]
		0_terrain="Mm"
		1_adjacent="(!,Xv,!,C*,K*,X*,Ql,Qx*,W*)"
		2_flag="base2"
		3_imagestem="mountains/basic-castle"
	[/cfg]
[/macro]

[macro]
	type="MOUNTAINS_2x4_NW_SE"
	[cfg]
		0_terrain="Mm"
		1_prob=18
		2_flag="base2"
		3_imagestem="mountains/basic_range3"
	[/cfg]
[/macro]

[macro]
	type="MOUNTAINS_2x4_SW_NE"
	[cfg]
		0_terrain="Mm"
		1_prob=26
		2_flag="base2"
		3_imagestem="mountains/basic_range4"
	[/cfg]
[/macro]

[macro]
	type="MOUNTAINS_1x3_NW_SE"
	[cfg]
		0_terrain="Mm"
		1_prob=20
		2_flag="base2"
		3_imagestem="mountains/basic_range1"
	[/cfg]
[/macro]

[macro]
	type="MOUNTAINS_1x3_SW_NE"
	[cfg]
		0_terrain="Mm"
		1_prob=20
		2_flag="base2"
		3_imagestem="mountains/basic_range2"
	[/cfg]
[/macro]

[macro]
	type="MOUNTAINS_2x2"
	[cfg]
		0_terrain="Mm"
		1_prob=40
		2_flag="base2"
		3_imagestem="mountains/basic5"
	[/cfg]
[/macro]

[macro]
	type="MOUNTAINS_2x2"
	[cfg]
		0_terrain="Mm"
		1_prob=30
		2_flag="base2"
		3_imagestem="mountains/basic6"
	[/cfg]
[/macro]

[macro]
	type="MOUNTAIN_SINGLE_RANDOM"
	[cfg]
		0_terrain="Mm"
		1_flag="base2"
		2_imagestem="mountains/basic"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_RESTRICTED3_F"
	[cfg]
		0_terrain="Md"
		1_adjacent="(!,Xv,!,C*,K*,X*,Ql,Qx*,W*)"
		2_flag="base2"
		3_imagestem="mountains/dry-castle-n"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_ROTATION_RESTRICTED2_F"
	[cfg]
		0_terrain="Md"
		1_adjacent="(!,Xv,!,C*,K*,X*,Ql,Qx*,W*)"
		2_flag="base2"
		3_imagestem="mountains/dry-castle"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_RESTRICTED2_F"
	[cfg]
		0_terrain="Md"
		1_adjacent="(!,Xv,!,C*,K*,X*,Ql,Qx*)"
		2_flag="base2"
		3_imagestem="mountains/dry-castle-n"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_ROTATION_RESTRICTED_F"
	[cfg]
		0_terrain="Md"
		1_adjacent="(!,Xv,!,C*,K*,X*,Ql,Qx*,W*)"
		2_flag="base2"
		3_imagestem="mountains/dry-castle"
	[/cfg]
[/macro]

[macro]
	type="MOUNTAINS_2x4_NW_SE"
	[cfg]
		0_terrain="Md"
		1_prob=18
		2_flag="base2"
		3_imagestem="mountains/dry_range3"
	[/cfg]
[/macro]

[macro]
	type="MOUNTAINS_2x4_SW_NE"
	[cfg]
		0_terrain="Md"
		1_prob=26
		2_flag="base2"
		3_imagestem="mountains/dry_range4"
	[/cfg]
[/macro]

[macro]
	type="MOUNTAINS_1x3_NW_SE"
	[cfg]
		0_terrain="Md"
		1_prob=20
		2_flag="base2"
		3_imagestem="mountains/dry_range1"
	[/cfg]
[/macro]

[macro]
	type="MOUNTAINS_1x3_SW_NE"
	[cfg]
		0_terrain="Md"
		1_prob=20
		2_flag="base2"
		3_imagestem="mountains/dry_range2"
	[/cfg]
[/macro]

[macro]
	type="MOUNTAINS_2x2"
	[cfg]
		0_terrain="Md"
		1_prob=40
		2_flag="base2"
		3_imagestem="mountains/dry5"
	[/cfg]
[/macro]

[macro]
	type="MOUNTAINS_2x2"
	[cfg]
		0_terrain="Md"
		1_prob=30
		2_flag="base2"
		3_imagestem="mountains/dry6"
	[/cfg]
[/macro]

[macro]
	type="MOUNTAIN_SINGLE_RANDOM"
	[cfg]
		0_terrain="Md"
		1_flag="base2"
		2_imagestem="mountains/dry"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_COMPLETE_F"
	[cfg]
		0_terrainlist="Mv"
		1_adjacent="(!,Xv,Mv,!,C*,K*,X*,Q*)"
		2_flag="base2"
		3_imagestem="mountains/volcano"
	[/cfg]
[/macro]

[macro]
	type="MOUNTAINS_2x2"
	[cfg]
		0_terrain="Ms"
		1_prob=15
		2_flag="base2"
		3_imagestem="mountains/snow5"
	[/cfg]
[/macro]

[macro]
	type="MOUNTAINS_2x2"
	[cfg]
		0_terrain="Ms"
		1_prob=25
		2_flag="base2"
		3_imagestem="mountains/snow6"
	[/cfg]
[/macro]

[macro]
	type="MOUNTAIN_SINGLE_RANDOM"
	[cfg]
		0_terrain="Ms"
		1_flag="base2"
		2_imagestem="mountains/snow"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_RANDOM"
	[cfg]
		0_terrainlist="Mm"
		1_imagestem="hills/regular"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_RANDOM"
	[cfg]
		0_terrainlist="Md"
		1_imagestem="hills/dry"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_RANDOM"
	[cfg]
		0_terrainlist="Ms"
		1_imagestem="hills/snow"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_LF"
	[cfg]
		0_terrainlist="*^Xm"
		1_layer=1
		2_flag="clouds"
		3_imagestem="mountains/cloud@V"
	[/cfg]
[/macro]

[macro]
	type="PEAKS_1x2_SW_NE"
	[cfg]
		0_terrain="*^Xm"
		1_prob=15
		2_flag="peaks"
		3_imagestem="mountains/peak_range1"
	[/cfg]
[/macro]

[macro]
	type="PEAKS_LARGE"
	[cfg]
		0_terrain="*^Xm"
		1_prob=25
		2_flag="peaks"
		3_imagestem="mountains/peak_large1"
	[/cfg]
[/macro]

[macro]
	type="PEAKS_LARGE"
	[cfg]
		0_terrain="*^Xm"
		1_prob=33
		2_flag="peaks"
		3_imagestem="mountains/peak_large2"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_RANDOM_LF"
	[cfg]
		0_terrainlist="*^Xm"
		1_layer=2
		2_flag="peaks"
		3_imagestem="mountains/peak"
	[/cfg]
[/macro]

[macro]
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Vh"
		1_imagestem="village/human"
	[/cfg]
[/macro]

[macro]
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Vha"
		1_imagestem="village/human-snow"
	[/cfg]
[/macro]

[macro]
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Vhr"
		1_imagestem="village/human-cottage-ruin"
	[/cfg]
[/macro]

[macro]
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Vhh"
		1_imagestem="village/human-hills"
	[/cfg]
[/macro]

[macro]
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Vhha"
		1_imagestem="village/human-snow-hills"
	[/cfg]
[/macro]

[macro]
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Vhhr"
		1_imagestem="village/human-hills-ruin"
	[/cfg]
[/macro]

[macro]
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Vhc"
		1_imagestem="village/human-city"
	[/cfg]
[/macro]

[macro]
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Vhca"
		1_imagestem="village/human-city-snow"
	[/cfg]
[/macro]

[macro]
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Vhcr"
		1_imagestem="village/human-city-ruin"
	[/cfg]
[/macro]

[macro]
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Vht"
		1_imagestem="village/tropical-forest"
	[/cfg]
[/macro]

[macro]
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Vc"
		1_imagestem="village/hut"
	[/cfg]
[/macro]

[macro]
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Vca"
		1_imagestem="village/hut-snow"
	[/cfg]
[/macro]

[macro]
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Vl"
		1_imagestem="village/log-cabin"
	[/cfg]
[/macro]

[macro]
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Vla"
		1_imagestem="village/log-cabin-snow"
	[/cfg]
[/macro]

[macro]
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Vct"
		1_imagestem="village/camp"
	[/cfg]
[/macro]

[macro]
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Vaa"
		1_imagestem="village/igloo"
	[/cfg]
[/macro]

[macro]
	postfix="{VILLAGE_PROBABILITY 20}"
	type="NEW:VILLAGE_A3"
	[cfg]
		0_terrainlist="*^Vd"
		1_time=200
		2_imagestem="village/drake1-A"
	[/cfg]
[/macro]

[macro]
	postfix="{VILLAGE_PROBABILITY 25}"
	type="NEW:VILLAGE_A3"
	[cfg]
		0_terrainlist="*^Vd"
		1_time=100
		2_imagestem="village/drake2-A"
	[/cfg]
[/macro]

[macro]
	postfix="{VILLAGE_PROBABILITY 33}"
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Vd"
		1_imagestem="village/drake3"
	[/cfg]
[/macro]

[macro]
	postfix="{VILLAGE_PROBABILITY 50}"
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Vd"
		1_imagestem="village/drake4"
	[/cfg]
[/macro]

[macro]
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Vd"
		1_imagestem="village/drake5"
	[/cfg]
[/macro]

[macro]
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Vo"
		1_imagestem="village/orc"
	[/cfg]
[/macro]

[macro]
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Voa"
		1_imagestem="village/orc-snow"
	[/cfg]
[/macro]

[macro]
	postfix="{VILLAGE_PROBABILITY 10}"
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Ve"
		1_imagestem="village/elven"
	[/cfg]
[/macro]

[macro]
	postfix="{VILLAGE_PROBABILITY 28}"
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Ve"
		1_imagestem="village/elven3"
	[/cfg]
[/macro]

[macro]
	postfix="{VILLAGE_PROBABILITY 38}"
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Ve"
		1_imagestem="village/elven4"
	[/cfg]
[/macro]

[macro]
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Ve"
		1_imagestem="village/elven2"
	[/cfg]
[/macro]

[macro]
	postfix="{VILLAGE_PROBABILITY 10}"
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Vea"
		1_imagestem="village/elven-snow"
	[/cfg]
[/macro]

[macro]
	postfix="{VILLAGE_PROBABILITY 28}"
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Vea"
		1_imagestem="village/elven-snow3"
	[/cfg]
[/macro]

[macro]
	postfix="{VILLAGE_PROBABILITY 38}"
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Vea"
		1_imagestem="village/elven-snow4"
	[/cfg]
[/macro]

[macro]
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Vea"
		1_imagestem="village/elven-snow2"
	[/cfg]
[/macro]

[macro]
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Vda"
		1_imagestem="village/desert"
	[/cfg]
[/macro]

[macro]
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Vdt"
		1_imagestem="village/desert-camp"
	[/cfg]
[/macro]

[macro]
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Vu"
		1_imagestem="village/cave"
	[/cfg]
[/macro]

[macro]
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Vud"
		1_imagestem="village/dwarven"
	[/cfg]
[/macro]

[macro]
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Vhs"
		1_imagestem="village/swampwater"
	[/cfg]
[/macro]

[macro]
	postfix="{VILLAGE_PROBABILITY 20}"
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Vm"
		1_imagestem="village/coast"
	[/cfg]
[/macro]

[macro]
	postfix="{VILLAGE_PROBABILITY 24}"
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Vm"
		1_imagestem="village/coast2"
	[/cfg]
[/macro]

[macro]
	postfix="{VILLAGE_PROBABILITY 29}"
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Vm"
		1_imagestem="village/coast3"
	[/cfg]
[/macro]

[macro]
	postfix="{VILLAGE_PROBABILITY 35}"
	type="NEW:VILLAGE"
	[cfg]
		0_terrainlist="*^Vm"
		1_imagestem="village/coast4"
	[/cfg]
[/macro]

[macro]
	type="NEW:VILLAGE_A4"
	[cfg]
		0_terrainlist="*^Vm"
		1_time=140
		2_imagestem="village/coast5-A"
	[/cfg]
[/macro]

[macro]
	type="NEW:FENCE"
	[cfg]
		0_terrainlist="*^Eff"
		1_imagestem="embellishments/fence"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_RANDOM_LF"
	[cfg]
		0_terrainlist="(*^Ufi,*^Ii)"
		1_layer=1
		2_flag="light"
		3_imagestem="cave/beam"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_RANDOM_L"
	[cfg]
		0_terrainlist="*^Gvs"
		1_layer=-81
		2_imagestem="embellishments/farm-veg-spring"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_COMPLETE_L"
	[cfg]
		0_terrainlist="*^Ewl"
		1_adjacent="(C*,K*,X*,Ql*,Qx*,G*,M*,*^V*,R*,A*,D*,U*,H*)"
		2_layer=-86
		3_imagestem="embellishments/water-lilies"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_COMPLETE_L"
	[cfg]
		0_terrainlist="*^Ewf"
		1_adjacent="(C*,K*,X*,Ql*,Qx*,G*,M*,*^V*,R*,A*,D*,U*,H*)"
		2_layer=-86
		3_imagestem="embellishments/water-lilies-flower"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_B"
	[cfg]
		0_terrainlist="*^Wm,*^Vwm"
		1_builder="ANIMATION_18_70"
		2_imagestem="misc/windmill"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_B"
	[cfg]
		0_terrainlist="*^Ecf"
		1_builder="ANIMATION_08"
		2_imagestem="misc/fire"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_RANDOM_L"
	[cfg]
		0_terrainlist="*^Efm"
		1_layer=-500
		2_imagestem="embellishments/flowers-mixed"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_RANDOM_L"
	[cfg]
		0_terrainlist="*^Dr"
		1_layer=-1
		2_imagestem="misc/rubble"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_RANDOM"
	[cfg]
		0_terrainlist="*^Es"
		1_imagestem="embellishments/stones-small"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_RANDOM"
	[cfg]
		0_terrainlist="*^Em"
		1_imagestem="embellishments/mushroom"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_COMPLETE"
	[cfg]
		0_terrainlist="*^Emf"
		1_adjacent="(C*,K*,X*,Ql*,Qx*,W*,M*,*^V*)"
		2_imagestem="embellishments/mushroom-farm"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_F"
	[cfg]
		0_terrainlist="Ql*^Bs\"
		1_flag="bridge"
		2_imagestem="cave/chasm-lava-stone-bridge-se-nw"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_F"
	[cfg]
		0_terrainlist="Ql*^Bs|"
		1_flag="bridge"
		2_imagestem="cave/chasm-lava-stone-bridge-s-n"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_F"
	[cfg]
		0_terrainlist="Ql*^Bs/"
		1_flag="bridge"
		2_imagestem="cave/chasm-lava-stone-bridge-sw-ne"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_F"
	[cfg]
		0_terrainlist="W*^Bs\,S*^Bs\"
		1_flag="bridge"
		2_imagestem="cave/chasm-water-stone-bridge-se-nw"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_F"
	[cfg]
		0_terrainlist="W*^Bs/,S*^Bs/"
		1_flag="bridge"
		2_imagestem="cave/chasm-water-stone-bridge-sw-ne"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_F"
	[cfg]
		0_terrainlist="*^Bs\"
		1_flag="bridge"
		2_imagestem="cave/chasm-stone-bridge-se-nw"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_F"
	[cfg]
		0_terrainlist="*^Bs|"
		1_flag="bridge"
		2_imagestem="cave/chasm-stone-bridge-s-n"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_F"
	[cfg]
		0_terrainlist="*^Bs/"
		1_flag="bridge"
		2_imagestem="cave/chasm-stone-bridge-sw-ne"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_P"
	[cfg]
		0_terrainlist="Gg"
		1_prob=20
		2_imagestem="grass/green"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_RANDOM"
	[cfg]
		0_terrainlist="Gg"
		1_imagestem="grass/green"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_P"
	[cfg]
		0_terrainlist="Gs"
		1_prob=25
		2_imagestem="grass/semi-dry"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_RANDOM"
	[cfg]
		0_terrainlist="Gs"
		1_imagestem="grass/semi-dry"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_P"
	[cfg]
		0_terrainlist="Gd"
		1_prob=25
		2_imagestem="grass/dry"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_RANDOM"
	[cfg]
		0_terrainlist="Gd"
		1_imagestem="grass/dry"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_RANDOM"
	[cfg]
		0_terrainlist="Gll"
		1_imagestem="grass/leaf-litter"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_RANDOM"
	[cfg]
		0_terrainlist="Re"
		1_imagestem="flat/dirt"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_RANDOM"
	[cfg]
		0_terrainlist="Rb"
		1_imagestem="flat/dirt-dark"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_RANDOM"
	[cfg]
		0_terrainlist="Rr"
		1_imagestem="flat/road"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_RANDOM"
	[cfg]
		0_terrainlist="Rrc"
		1_imagestem="flat/road-clean"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_RANDOM"
	[cfg]
		0_terrainlist="Rp"
		1_imagestem="flat/stone-path"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_RANDOM"
	[cfg]
		0_terrainlist="Hh"
		1_imagestem="hills/regular"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_RANDOM"
	[cfg]
		0_terrainlist="Hhd"
		1_imagestem="hills/dry"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_RANDOM"
	[cfg]
		0_terrainlist="Uu"
		1_imagestem="cave/floor"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_RANDOM"
	[cfg]
		0_terrainlist="Uue"
		1_imagestem="cave/earthy-floor"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_RANDOM"
	[cfg]
		0_terrainlist="Ur"
		1_imagestem="cave/path"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_RANDOM"
	[cfg]
		0_terrainlist="Urb"
		1_imagestem="cave/flagstones-dark"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_RANDOM"
	[cfg]
		0_terrainlist="Uh"
		1_imagestem="cave/hills-variation"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE"
	[cfg]
		0_terrainlist="Xu*"
		1_imagestem="cave/wall-rough"
	[/cfg]
[/macro]

[macro]
	type="KEEP_BASE"
	[cfg]
		0_terrainlist="Xo*"
		1_imagestem="walls/wall-stone-base"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_RANDOM"
	[cfg]
		0_terrainlist="Qxua"
		1_imagestem="chasm/abyss"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE"
	[cfg]
		0_terrainlist="Qx*"
		1_imagestem="chasm/depths"
	[/cfg]
[/macro]

[macro]
	type="ANIMATED_WATER_15"
	[cfg]
		0_terrainlist="Ql,Qlf"
		1_ipf="()"
		2_time=150
		3_imagestem="unwalkable/lava"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_RANDOM_L"
	[cfg]
		0_terrainlist="Wwr,Wwrt,Wwrg"
		1_layer=-320
		2_imagestem="water/reef"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_RANDOM"
	[cfg]
		0_terrainlist="Dd"
		1_imagestem="sand/desert"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_RESTRICTED_P"
	[cfg]
		0_terrain="*^Edp,*^Edpp"
		1_adjacent="(C*,K*,Q*)"
		2_prob=33
		3_imagestem="embellishments/desert-plant"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_RESTRICTED_P"
	[cfg]
		0_terrain="*^Edp,*^Edpp"
		1_adjacent="(C*,K*,Q*)"
		2_prob=50
		3_imagestem="embellishments/desert-plant2"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_RESTRICTED_P"
	[cfg]
		0_terrain="*^Edp,*^Edpp"
		1_adjacent="(C*,K*,Q*)"
		2_prob=100
		3_imagestem="embellishments/desert-plant3"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_P"
	[cfg]
		0_terrainlist="*^Edp,*^Edpp"
		1_prob=9
		2_imagestem="embellishments/desert-plant"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_P"
	[cfg]
		0_terrainlist="*^Edp,*^Edpp"
		1_prob=10
		2_imagestem="embellishments/desert-plant2"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_P"
	[cfg]
		0_terrainlist="*^Edp,*^Edpp"
		1_prob=11
		2_imagestem="embellishments/desert-plant3"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_P"
	[cfg]
		0_terrainlist="*^Edp,*^Edpp"
		1_prob=12
		2_imagestem="embellishments/desert-plant4"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_P"
	[cfg]
		0_terrainlist="*^Edp"
		1_prob=14
		2_imagestem="embellishments/desert-plant5"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_P"
	[cfg]
		0_terrainlist="*^Edp,*^Edpp"
		1_prob=16
		2_imagestem="embellishments/desert-plant6"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_P"
	[cfg]
		0_terrainlist="*^Edp"
		1_prob=20
		2_imagestem="embellishments/desert-plant7"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_P"
	[cfg]
		0_terrainlist="*^Edp,*^Edpp"
		1_prob=24
		2_imagestem="embellishments/desert-plant8"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_P"
	[cfg]
		0_terrainlist="*^Edp,*^Edpp"
		1_prob=32
		2_imagestem="embellishments/desert-plant9"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_P"
	[cfg]
		0_terrainlist="*^Edp,*^Edpp"
		1_prob=48
		2_imagestem="embellishments/desert-plant10"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_P"
	[cfg]
		0_terrainlist="*^Edp,*^Edpp"
		1_prob=100
		2_imagestem="embellishments/desert-plant11"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_RANDOM"
	[cfg]
		0_terrainlist="*^Esd"
		1_imagestem="embellishments/rocks"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_RANDOM"
	[cfg]
		0_terrainlist="Ds"
		1_imagestem="sand/beach"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_RANDOM"
	[cfg]
		0_terrainlist="Rd"
		1_imagestem="flat/desert-road"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_F"
	[cfg]
		0_terrainlist="*^Dc"
		1_flag="overlay"
		2_imagestem="sand/crater"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_RANDOM"
	[cfg]
		0_terrainlist="Hd"
		1_imagestem="hills/desert"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_RANDOM"
	[cfg]
		0_terrainlist="Aa"
		1_imagestem="frozen/snow"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_P"
	[cfg]
		0_terrainlist="Ai"
		1_prob=10
		2_imagestem="frozen/ice2"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_P"
	[cfg]
		0_terrainlist="Ai"
		1_prob=11
		2_imagestem="frozen/ice3"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_P"
	[cfg]
		0_terrainlist="Ai"
		1_prob=13
		2_imagestem="frozen/ice5"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_P"
	[cfg]
		0_terrainlist="Ai"
		1_prob=14
		2_imagestem="frozen/ice6"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_P"
	[cfg]
		0_terrainlist="Ai"
		1_prob=42
		2_imagestem="frozen/ice4"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE"
	[cfg]
		0_terrainlist="Ai"
		1_imagestem="frozen/ice"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_RANDOM"
	[cfg]
		0_terrainlist="Ha"
		1_imagestem="hills/snow"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_RANDOM"
	[cfg]
		0_terrainlist="Iwr"
		1_imagestem="interior/wood-regular"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_RANDOM_LF"
	[cfg]
		0_terrainlist="Wwf"
		1_layer=-519
		2_flag="ford"
		3_imagestem="water/ford"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_RANDOM"
	[cfg]
		0_terrainlist="Sm"
		1_imagestem="swamp/mud"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_P"
	[cfg]
		0_terrainlist="Ss"
		1_prob=33
		2_imagestem="swamp/water-plant@V"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_RANDOM"
	[cfg]
		0_terrainlist="Ss,Chs"
		1_imagestem="swamp/water"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_SINGLEHEX_B"
	[cfg]
		0_terrainlist="(Wo)"
		1_builder="ANIMATION_15_SLOW"
		2_imagestem="water/ocean"
	[/cfg]
[/macro]

[macro]
	type="ANIMATED_WATER_15"
	[cfg]
		0_terrainlist="(Wot)"
		1_ipf="~CS(-45,-5,25)"
		2_time=150
		3_imagestem="water/ocean"
	[/cfg]
[/macro]

[macro]
	type="ANIMATED_WATER_15"
	[cfg]
		0_terrainlist="(Wog)"
		1_ipf="~CS(15,0,-30)"
		2_time=150
		3_imagestem="water/ocean"
	[/cfg]
[/macro]

[macro]
	type="ANIMATED_WATER_15"
	[cfg]
		0_terrainlist="(Ww,Wwr,Wwf)"
		1_ipf="~CS(40,0,-30)"
		2_time=110
		3_imagestem="water/coast-tropical"
	[/cfg]
[/macro]

[macro]
	type="ANIMATED_WATER_15"
	[cfg]
		0_terrainlist="(Wwg,Wwrg)"
		1_ipf="~CS(60,0,-55)"
		2_time=110
		3_imagestem="water/coast-tropical"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_SINGLEHEX_B"
	[cfg]
		0_terrainlist="(Wwt,Wwrt)"
		1_builder="ANIMATION_15"
		2_imagestem="water/coast-tropical"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE"
	[cfg]
		0_terrainlist="Ch,Chr,Cha"
		1_imagestem="flat/road"
	[/cfg]
[/macro]

[macro]
	type="KEEP_BASE"
	[cfg]
		0_terrainlist="Kh*"
		1_imagestem="castle/cobbles-keep"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE"
	[cfg]
		0_terrainlist="Chw"
		1_imagestem="castle/sunken-ruin"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE"
	[cfg]
		0_terrainlist="Cv"
		1_imagestem="castle/elven/grounds"
	[/cfg]
[/macro]

[macro]
	type="KEEP_BASE"
	[cfg]
		0_terrainlist="Kv"
		1_imagestem="castle/elven/keep"
	[/cfg]
[/macro]

[macro]
	type="KEEP_BASE_RANDOM"
	[cfg]
		0_terrainlist="Ket"
		1_imagestem="interior/wood-tan"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_RANDOM"
	[cfg]
		0_terrainlist="Ket"
		1_imagestem="interior/wood-tan-debris"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE"
	[cfg]
		0_terrainlist="(Ce*,Ke*)"
		1_imagestem="flat/dirt"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_RANDOM"
	[cfg]
		0_terrainlist="Ke"
		1_imagestem="castle/encampment/tent"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY_RANDOM"
	[cfg]
		0_terrainlist="Kea"
		1_imagestem="castle/encampment/tent-snow"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE"
	[cfg]
		0_terrainlist="Ea"
		1_imagestem="castle/economy-area"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE"
	[cfg]
		0_terrainlist="Co*"
		1_imagestem="flat/dirt-dark"
	[/cfg]
[/macro]

[macro]
	type="KEEP_BASE"
	[cfg]
		0_terrainlist="Ko"
		1_imagestem="castle/orcish/keep"
	[/cfg]
[/macro]

[macro]
	type="KEEP_BASE"
	[cfg]
		0_terrainlist="Koa"
		1_imagestem="castle/winter-orcish/keep"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE"
	[cfg]
		0_terrainlist="Cud"
		1_imagestem="castle/dwarven-castle-floor"
	[/cfg]
[/macro]

[macro]
	type="KEEP_BASE"
	[cfg]
		0_terrainlist="Kud"
		1_imagestem="castle/dwarven-keep-floor"
	[/cfg]
[/macro]

[macro]
	type="OVERLAY"
	[cfg]
		0_terrainlist="Kud"
		1_imagestem="castle/dwarven-keep"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE"
	[cfg]
		0_terrainlist="Cd,Cdr"
		1_imagestem="castle/sand/dirt"
	[/cfg]
[/macro]

[macro]
	type="KEEP_BASE"
	[cfg]
		0_terrainlist="Kd,Kdr"
		1_imagestem="castle/sand/cobbles"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL"
	[cfg]
		0_terrainlist="Cv"
		1_adjacent="(!,C*,K*)"
		2_imagestem="castle/elven/castle"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL2"
	[cfg]
		0_terrainlist="Kv"
		1_adjacent1="!,Ket,!,C*,Ke*"
		2_adjacent2="(!,C*,K*)"
		3_imagestem="castle/elven/keep-castle"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL"
	[cfg]
		0_terrainlist="Kv"
		1_adjacent="(Ke,Kea,!,K*)"
		2_imagestem="castle/elven/keep"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL"
	[cfg]
		0_terrainlist="Co,Cv"
		1_adjacent="(!,C*,K*)"
		2_imagestem="castle/orcish/fort"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL2"
	[cfg]
		0_terrainlist="Ko,Kv"
		1_adjacent1="!,Ket,!,C*,Ke*"
		2_adjacent2="(!,C*,K*)"
		3_imagestem="castle/orcish/keep-fort"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL"
	[cfg]
		0_terrainlist="Ko,Kv"
		1_adjacent="(!,K*)"
		2_imagestem="castle/orcish/keep"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL"
	[cfg]
		0_terrainlist="(Co*,Cv)"
		1_adjacent="(!,C*,K*)"
		2_imagestem="castle/winter-orcish/fort"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL2"
	[cfg]
		0_terrainlist="(Ko*,Kv)"
		1_adjacent1="!,Ket,!,C*,Ke*"
		2_adjacent2="(!,C*,K*)"
		3_imagestem="castle/winter-orcish/keep-fort"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL"
	[cfg]
		0_terrainlist="(Ko*,Kv)"
		1_adjacent="(Ke,Kea,!,K*)"
		2_imagestem="castle/winter-orcish/keep"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL"
	[cfg]
		0_terrainlist="Cd,Cv,Co*"
		1_adjacent="(!,C*,K*)"
		2_imagestem="castle/sand/castle"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL2"
	[cfg]
		0_terrainlist="Kd,Kv,Ko*"
		1_adjacent1="!,Ket,!,C*,Ke*"
		2_adjacent2="(!,C*,K*)"
		3_imagestem="castle/sand/keep-castle"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL"
	[cfg]
		0_terrainlist="Kd,Ko*,Kv"
		1_adjacent="(!,K*)"
		2_imagestem="castle/sand/keep"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL"
	[cfg]
		0_terrainlist="Cd*,Cv,Co*"
		1_adjacent="(!,C*,K*)"
		2_imagestem="castle/sand/ruin-castle"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL2"
	[cfg]
		0_terrainlist="Kdr,Kv,Ko*"
		1_adjacent1="!,Ket,!,C*,Ke*"
		2_adjacent2="(!,C*,K*)"
		3_imagestem="castle/sand/ruin-keep-castle"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL"
	[cfg]
		0_terrainlist="Kd*,Ko*,Kv"
		1_adjacent="(Ke,Kea,!,K*)"
		2_imagestem="castle/sand/ruin-keep"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL"
	[cfg]
		0_terrainlist="Ch,Cv,Co,Cd"
		1_adjacent="(!,C*,K*)"
		2_imagestem="castle/castle"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL2"
	[cfg]
		0_terrainlist="Kh,Kv,Ko,Kd"
		1_adjacent1="!,Ket,!,C*,Ke*"
		2_adjacent2="(!,C*,K*)"
		3_imagestem="castle/keep-castle"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL"
	[cfg]
		0_terrainlist="Kh,Kd*,Ko,Kv"
		1_adjacent="(!,K*)"
		2_imagestem="castle/keep"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL"
	[cfg]
		0_terrainlist="Ch,Cha,Coa"
		1_adjacent="(!,C*,K*)"
		2_imagestem="castle/snowy/castle"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL2"
	[cfg]
		0_terrainlist="Kh,Kha,Koa"
		1_adjacent1="!,Ket,!,C*,Ke*"
		2_adjacent2="(!,C*,K*)"
		3_imagestem="castle/snowy/keep-castle"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL"
	[cfg]
		0_terrainlist="Kh,Kha,Kd*,Ko*,Kv"
		1_adjacent="(Ke,Kea,!,K*)"
		2_imagestem="castle/snowy/keep"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL"
	[cfg]
		0_terrainlist="(!,Chr,Chs,Ce*,Ke*,!,Ch*)"
		1_adjacent="(W*)"
		2_imagestem="castle/sunken-ruin"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL2_P"
	[cfg]
		0_terrainlist="Khw,Khs"
		1_adjacent1="Ch*"
		2_adjacent2="W*,Ss"
		3_prob=75
		4_imagestem="castle/sunken-ruinkeep1-castle"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL2"
	[cfg]
		0_terrainlist="Khw,Khs"
		1_adjacent1="Ch*"
		2_adjacent2="W*,Ss"
		3_imagestem="castle/sunkenkeep-castle"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL_P"
	[cfg]
		0_terrainlist="(!,Khr,!,Kh*)"
		1_adjacent="W*,Ss,Chw,Chs"
		2_prob=75
		3_imagestem="castle/sunken-ruinkeep1"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL"
	[cfg]
		0_terrainlist="(!,Khr,!,Kh*)"
		1_adjacent="W*,Ss,Chw,Chs"
		2_imagestem="castle/sunkenkeep"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL"
	[cfg]
		0_terrainlist="Ch*,Cd*,Cv,Co*"
		1_adjacent="(!,C*,K*)"
		2_imagestem="castle/ruin"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL2_P"
	[cfg]
		0_terrainlist="(Khr,Khw,Khs)"
		1_adjacent1="(C*)"
		2_adjacent2="(!,Ch*,Kh*)"
		3_prob=75
		4_imagestem="castle/ruinkeep1-castle"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL2"
	[cfg]
		0_terrainlist="(Khr,Khw,Khs)"
		1_adjacent1="(C*)"
		2_adjacent2="(!,Ch*,Kh*)"
		3_imagestem="castle/keep-castle"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL_P"
	[cfg]
		0_terrainlist="Kh*,Kd*,Ko*,Kv"
		1_adjacent="(Ke,Kea,!,K*)"
		2_prob=75
		3_imagestem="castle/ruinkeep1"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL"
	[cfg]
		0_terrainlist="Kh*,Kd*,Ko*,Kv"
		1_adjacent="(Ke,Kea,!,K*)"
		2_imagestem="castle/keep"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL2"
	[cfg]
		0_terrainlist="!,Ke,Kea,Kud,!,K*"
		1_adjacent1="(Ke,Kea,C*)"
		2_adjacent2="(!,C*,K*)"
		3_imagestem="castle/encampment/tall-keep-castle"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL"
	[cfg]
		0_terrainlist="!,Ke,Kea,Kud,!,K*"
		1_adjacent="(Ke,Kea,!,K*)"
		2_imagestem="castle/encampment/tall-keep"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL"
	[cfg]
		0_terrainlist="(!,Cud,Cea,Coa,Cha,Kud,Kea,Koa,Kha,!,C*,K*)"
		1_adjacent="(!,C*,K*)"
		2_imagestem="castle/encampment/regular"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL"
	[cfg]
		0_terrainlist="(!,Cud,Kud,!,C*,K*)"
		1_adjacent="(!,C*,K*)"
		2_imagestem="castle/encampment/snow"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="Chs"
		1_adjacent="!,S*,W*,H*,M*,A*,Chs,K*"
		2_layer=-230
		3_imagestem="swamp/water"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="Ch,Chr,Cha"
		1_adjacent="!,Ch,Chr,Cha,Ket,!,Ke*,C*"
		2_layer=-300
		3_imagestem="flat/road"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_LF"
	[cfg]
		0_terrainlist="!,Ket,Cd*,!,C*,Ke*"
		1_adjacent="Cd*"
		2_layer=-360
		3_flag="inside"
		4_imagestem="flat/desert-road"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="Cd*"
		1_adjacent="!,Ket,Cd*,!,C*,Ke*"
		2_layer=-360
		3_imagestem="flat/desert-road"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_LF"
	[cfg]
		0_terrainlist="!,Ce*,Ke*,!,C*"
		1_adjacent="!,Ket,!,Ce*,Ke*"
		2_layer=-370
		3_flag="inside"
		4_imagestem="flat/dirt"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="!,Ket,!,Ce*,Ke*"
		1_adjacent="!,Ce*,Ke*,!,C*"
		2_layer=-370
		3_imagestem="flat/dirt"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_LF"
	[cfg]
		0_terrainlist="!,Ket,Co*,!,C*,Ke*"
		1_adjacent="Co*"
		2_layer=-380
		3_flag="inside"
		4_imagestem="flat/dirt-dark"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="Co*"
		1_adjacent="!,Ket,Co*,!,C*,Ke*"
		2_layer=-380
		3_imagestem="flat/dirt-dark"
	[/cfg]
[/macro]

[macro]
	type="DISABLE_BASE_TRANSITIONS"
	[cfg]
		0_terrainlist="Qx*,Ql*,Xu*,Cud,Kud"
	[/cfg]
[/macro]

[macro]
	type="WALL_TRANSITION3"
	[cfg]
		0_terrainlist1="(Cud,Kud)"
		1_terrainlist2="Ql*"
		2_terrainlist3="Qx*"
		3_imagestem="unwalkable/dcastle-lava-chasm"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL2"
	[cfg]
		0_terrainlist="(Cud,Kud)"
		1_adjacent1="X*"
		2_adjacent2="(!,Cud,Kud,X*)"
		3_imagestem="castle/dwarven-castle-wall"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL2"
	[cfg]
		0_terrainlist="(Cud,Kud)"
		1_adjacent1="Ql*"
		2_adjacent2="(!,Cud,Kud,Ql*)"
		3_imagestem="unwalkable/dcastle-lava"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL2"
	[cfg]
		0_terrainlist="(Cud,Kud)"
		1_adjacent1="Qx*"
		2_adjacent2="(!,Cud,Kud,Qx*)"
		3_imagestem="unwalkable/dcastle-chasm"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL"
	[cfg]
		0_terrainlist="(Cud,Kud)"
		1_adjacent="(!,Cud,Kud,X*)"
		2_imagestem="castle/dwarven-castle"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL"
	[cfg]
		0_terrainlist="Xuce"
		1_adjacent="(Qx*,Ql*)"
		2_imagestem="cave/earthy-wall-rough-chasm"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL"
	[cfg]
		0_terrainlist="Xuce"
		1_adjacent="(!,Xu*)"
		2_imagestem="cave/earthy-wall-hewn"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL"
	[cfg]
		0_terrainlist="Xue,Xuce"
		1_adjacent="(Qx*,Ql*)"
		2_imagestem="cave/earthy-wall-rough-chasm"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL"
	[cfg]
		0_terrainlist="Xue,Xuce"
		1_adjacent="(!,Xu*)"
		2_imagestem="cave/earthy-wall-rough"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL"
	[cfg]
		0_terrainlist="Xuc,Xue,Xuce"
		1_adjacent="(Qx*,Ql*)"
		2_imagestem="cave/wall-rough-chasm"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL"
	[cfg]
		0_terrainlist="Xuc,Xue,Xuce"
		1_adjacent="(!,Xu*)"
		2_imagestem="cave/wall-hewn"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL"
	[cfg]
		0_terrainlist="Xu*"
		1_adjacent="(Qx*,Ql*)"
		2_imagestem="cave/wall-rough-chasm"
	[/cfg]
[/macro]

[macro]
	type="NEW:WALL"
	[cfg]
		0_terrainlist="Xu*"
		1_adjacent="(!,Xu*)"
		2_imagestem="cave/wall-rough"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_LF"
	[cfg]
		0_terrainlist="(Cha,Kha,Coa,Koa,Cea,Kea)"
		1_adjacent="Qx*"
		2_layer=-89
		3_flag="transition2"
		4_imagestem="chasm/regular-snow-castle"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_LF"
	[cfg]
		0_terrainlist="(!,Cud,Kud,!,C*,K*)"
		1_adjacent="Qxe"
		2_layer=-89
		3_flag="transition2"
		4_imagestem="chasm/earthy-castle"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_LF"
	[cfg]
		0_terrainlist="(!,Cud,Kud,!,C*,K*)"
		1_adjacent="Qx*"
		2_layer=-89
		3_flag="transition2"
		4_imagestem="chasm/regular-castle"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_LF"
	[cfg]
		0_terrainlist="(!,Cud,Kud,!,C*,K*)"
		1_adjacent="Ql*"
		2_layer=-89
		3_flag="transition2"
		4_imagestem="unwalkable/castle-lava-chasm"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_LF"
	[cfg]
		0_terrainlist="(Qx*,Ql*)"
		1_adjacent="(M*,Mv)"
		2_layer=2
		3_flag="transition3"
		4_imagestem="mountains/blend-from-chasm"
	[/cfg]
[/macro]

[macro]
	type="WALL_TRANSITION2_LF"
	[cfg]
		0_terrainlist1="Ql"
		1_terrainlist2="Qx*,Xv,_off^_usr"
		2_adjacent="(!,Ql,Qx*)"
		3_layer=-90
		4_flag="ground"
		5_imagestem="unwalkable/lava-chasm"
	[/cfg]
[/macro]

[macro]
	type="WALL_TRANSITION2_LF"
	[cfg]
		0_terrainlist1="Qlf"
		1_terrainlist2="Qx*,Xv,_off^_usr"
		2_adjacent="(!,Ql*,Qx*)"
		3_layer=-90
		4_flag="ground"
		5_imagestem="unwalkable/lava-chasm"
	[/cfg]
[/macro]

[macro]
	type="WALL_TRANSITION_LF"
	[cfg]
		0_terrainlist="Ql"
		1_adjacent="(!,Ql,Xv,_off^_usr)"
		2_layer=-90
		3_flag="ground"
		4_imagestem="unwalkable/lava"
	[/cfg]
[/macro]

[macro]
	type="WALL_TRANSITION_LF"
	[cfg]
		0_terrainlist="Qlf"
		1_adjacent="(!,Qlf,Xv,_off^_usr)"
		2_layer=-90
		3_flag="ground"
		4_imagestem="unwalkable/lava-high"
	[/cfg]
[/macro]

[macro]
	type="WALL_TRANSITION_LF"
	[cfg]
		0_terrainlist="Qx*"
		1_adjacent="(Ai*,Aa*,Ha*,Ms*,)"
		2_layer=-90
		3_flag="ground"
		4_imagestem="chasm/regular-snow"
	[/cfg]
[/macro]

[macro]
	type="WALL_TRANSITION_LF"
	[cfg]
		0_terrainlist="Qx*"
		1_adjacent="(W*,S*)"
		2_layer=-90
		3_flag="ground"
		4_imagestem="chasm/water"
	[/cfg]
[/macro]

[macro]
	type="WALL_TRANSITION_LF"
	[cfg]
		0_terrainlist="Qxe"
		1_adjacent="(!,Qx*,Xv,_off^_usr)"
		2_layer=-90
		3_flag="ground"
		4_imagestem="chasm/earthy"
	[/cfg]
[/macro]

[macro]
	type="WALL_TRANSITION_LF"
	[cfg]
		0_terrainlist="Qx*"
		1_adjacent="(!,Qx*,Xv,_off^_usr)"
		2_layer=-90
		3_flag="ground"
		4_imagestem="chasm/regular"
	[/cfg]
[/macro]

[macro]
	type="WALL_ADJACENT_TRANSITION"
	[cfg]
		0_terrainlist="Xol"
		1_adjacent="(!,,Xo*,Xu*)"
		2_builder="ANIMATION_10"
		3_imagestem="walls/wall-stone-lit"
	[/cfg]
[/macro]

[macro]
	type="WALL_ADJACENT_TRANSITION"
	[cfg]
		0_terrainlist="Xo*"
		1_adjacent="(!,,Xo*,Xu*)"
		2_builder="IMAGE_SINGLE"
		3_imagestem="walls/wall-stone"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="(Md,Mv)"
		1_adjacent="(!,Md,Hhd,Mv,W*,S*)"
		2_layer=-166
		3_imagestem="mountains/dry"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_LF"
	[cfg]
		0_terrainlist="(Hd,Hhd,Rb,Re,Rd,D*,Gd,Ha,A*,U*,Ql*)"
		1_adjacent="Mm"
		2_layer=0
		3_flag="inside"
		4_imagestem="mountains/blend-from-dry"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_LF"
	[cfg]
		0_terrainlist="(Hd,Hhd,Rb,Re,Rd,D*,Gd,U*,Ql*)"
		1_adjacent="Ms"
		2_layer=0
		3_flag="inside"
		4_imagestem="mountains/blend-from-dry"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="(Mm)"
		1_adjacent="(Hd,Hhd,Rb,Re,Rd,D*,Gd,Ha,A*,U*,Ql*)"
		2_layer=-166
		3_imagestem="hills/dry"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="(Ms)"
		1_adjacent="(Hd,Hhd,Rb,Re,Rd,D*,Gd,U*,Ql*)"
		2_layer=-166
		3_imagestem="hills/dry"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="(Ms,Ha)"
		1_adjacent="Hh*"
		2_layer=-170
		3_imagestem="hills/snow-to-hills"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="(Ms,Ha)"
		1_adjacent="(W*,S*)"
		2_layer=-171
		3_imagestem="hills/snow-to-water"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="(Ms,Ha)"
		1_adjacent="(!,Ha,Qx*,Mm,Ms,Md)"
		2_layer=-172
		3_imagestem="hills/snow"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="(Mm,Hh)"
		1_adjacent="(!,Hh,W*,S*)"
		2_layer=-180
		3_imagestem="hills/regular"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="(Hhd)"
		1_adjacent="(!,Hhd,W*,S*)"
		2_layer=-183
		3_imagestem="hills/dry"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="Hd"
		1_adjacent="(!,Hd,Qx*,W*)"
		2_layer=-184
		3_imagestem="hills/desert"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="Uh"
		1_adjacent="(!,Uh,W*,Ai)"
		2_layer=-200
		3_imagestem="cave/hills"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="(Uu,Uh)"
		1_adjacent="(!,Uu,Uh,W*,Ai)"
		2_layer=-220
		3_imagestem="cave/floor"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="(Uue)"
		1_adjacent="(!,Uue,W*,Ai)"
		2_layer=-221
		3_imagestem="cave/earthy-floor"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_LF"
	[cfg]
		0_terrainlist="Ai,W*,S*"
		1_adjacent="Ur"
		2_layer=-223
		3_flag="inside"
		4_imagestem="cave/floor"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="Urb"
		1_adjacent="(!,Urb)"
		2_layer=-224
		3_imagestem="cave/flagstones-dark"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="Ss"
		1_adjacent="(!,Ss,H*,M*,A*,Chs,K*)"
		2_layer=-230
		3_imagestem="swamp/water"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="Iwr"
		1_adjacent="G*,R*,W*,S*,D*,A*,Q*,Ur"
		2_layer=-230
		3_imagestem="interior/wood-regular"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_LF"
	[cfg]
		0_terrainlist="*^Efm"
		1_adjacent="G*"
		2_layer=-240
		3_flag="transition4"
		4_imagestem="embellishments/flowers-mixed"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_LF"
	[cfg]
		0_terrainlist="Gs"
		1_adjacent="Gg,Gd,Gll,Re,Rb,Rd,Rp"
		2_layer=-250
		3_flag="inside"
		4_imagestem="grass/semi-dry-long"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_LF"
	[cfg]
		0_terrainlist="Gg"
		1_adjacent="Gs,Gd,Gll,Re,Rb,Rd,Rp"
		2_layer=-251
		3_flag="inside"
		4_imagestem="grass/green-long"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_LF"
	[cfg]
		0_terrainlist="Gd"
		1_adjacent="Gg,Gs,Gll,Re,Rb,Rd,Rp"
		2_layer=-252
		3_flag="inside"
		4_imagestem="grass/dry-long"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_LF"
	[cfg]
		0_terrainlist="Gll"
		1_adjacent="Gg,Gs,Gd,Re,Rb,Rd,Rp"
		2_layer=-253
		3_flag="inside"
		4_imagestem="grass/leaf-litter-long"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="Gll"
		1_adjacent="Gg,Gs,Gd"
		2_layer=-254
		3_imagestem="grass/leaf-litter-long"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="Gd"
		1_adjacent="Gg,Gs,Gll"
		2_layer=-255
		3_imagestem="grass/dry-long"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="Gg"
		1_adjacent="Gs,Gd,Gll"
		2_layer=-256
		3_imagestem="grass/green-long"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="Gs"
		1_adjacent="Gg,Gd,Gll"
		2_layer=-257
		3_imagestem="grass/semi-dry-long"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="Gs"
		1_adjacent="(R*,D*,Aa,Ur)"
		2_layer=-260
		3_imagestem="grass/semi-dry-medium"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="Gg"
		1_adjacent="(R*,D*,Aa,Ur)"
		2_layer=-261
		3_imagestem="grass/green-medium"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="Gd"
		1_adjacent="(R*,D*,Aa,Ur)"
		2_layer=-262
		3_imagestem="grass/dry-medium"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="Gll"
		1_adjacent="(!,Gll,Qx*,W*,Ai,C*,K*)"
		2_layer=-270
		3_imagestem="grass/leaf-litter"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="(Gg*,Qx*)"
		1_adjacent="(!,Gg*,Qx*,Mm,Ms,Hh,C*,K*)"
		2_layer=-271
		3_imagestem="grass/green-abrupt"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="Gs"
		1_adjacent="(!,Gs,Qx*,C*,K*)"
		2_layer=-272
		3_imagestem="grass/semi-dry-abrupt"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="Gd"
		1_adjacent="(!,Gd,Qx*,C*,K*)"
		2_layer=-273
		3_imagestem="grass/dry-abrupt"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="Aa"
		1_adjacent="(W*,Ss)"
		2_layer=-280
		3_imagestem="frozen/snow-to-water"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="Aa"
		1_adjacent="(!,Aa,Qx*,G*)"
		2_layer=-281
		3_imagestem="frozen/snow"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="(W*,Ai)"
		1_adjacent="(R*,Gll)"
		2_layer=-300
		3_imagestem="flat/bank"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="(Sm)"
		1_adjacent="(R*,D*,Xv,_off^_usr)"
		2_layer=-310
		3_imagestem="swamp/mud-to-land"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="Dd"
		1_adjacent="R*"
		2_layer=-319
		3_imagestem="sand/desert"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="Ds"
		1_adjacent="R*"
		2_layer=-319
		3_imagestem="sand/beach"
	[/cfg]
[/macro]

[macro]
	type="NEW:WAVES"
	[cfg]
		0_terrainlist="D*,Hd"
		1_adjacent="W*"
		2_layer=-499
		3_imagestem="water/waves"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="Rr"
		1_adjacent="(!,Rr,W*,Ai)"
		2_layer=-320
		3_imagestem="flat/road"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="Rrc"
		1_adjacent="(!,Rr,W*,Ai)"
		2_layer=-321
		3_imagestem="flat/road-clean"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="Rp"
		1_adjacent="(!,Rp,W*,Ai)"
		2_layer=-322
		3_imagestem="flat/stone-path"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_LF"
	[cfg]
		0_terrainlist="*^Gvs"
		1_adjacent="(!,*^Gvs,C*,K*,*^F*,M*,H*,W*)"
		2_layer=-330
		3_flag="transition2"
		4_imagestem="embellishments/farm-veg-spring"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_LF"
	[cfg]
		0_terrainlist="*^Emf"
		1_adjacent="(!,*^Emf,C*,K*,*^F*,M*,H*,W*)"
		2_layer=-330
		3_flag="transition2"
		4_imagestem="embellishments/mushroom-farm"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="Ds"
		1_adjacent="(!,Ds,W*,S*,Ai)"
		2_layer=-510
		3_imagestem="sand/beach"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="Dd"
		1_adjacent="(!,R*,Dd,W*,S*,Ai)"
		2_layer=-510
		3_imagestem="sand/desert"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_LF"
	[cfg]
		0_terrainlist="(!,Rd,Rr*,Hh*,M*,Q*,D*)"
		1_adjacent="Rd"
		2_layer=-370
		3_flag="inside"
		4_imagestem="flat/desert-road"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="Rd"
		1_adjacent="(!,Rd,W*,Ai,Q*,D*)"
		2_layer=-371
		3_imagestem="flat/desert-road"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_LF"
	[cfg]
		0_terrainlist="(!,Re,Rr*,Hh*,M*,Q*,D*)"
		1_adjacent="Re"
		2_layer=-379
		3_flag="inside"
		4_imagestem="flat/dirt"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="Re"
		1_adjacent="(!,Re,Rr*,W*,Ai,Q*,D*)"
		2_layer=-380
		3_imagestem="flat/dirt"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_LF"
	[cfg]
		0_terrainlist="(!,Rb,Rr*,W*,Ai,Q*,D*)"
		1_adjacent="Rb"
		2_layer=-384
		3_flag="inside"
		4_imagestem="flat/dirt-dark"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="Rb"
		1_adjacent="(!,Rb,Rr*,W*,Ai,Q*,D*)"
		2_layer=-388
		3_imagestem="flat/dirt-dark"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_LF"
	[cfg]
		0_terrainlist="(!,Chw,Khw,Khs,!,C*,K*)"
		1_adjacent="(Ai,W*)"
		2_layer=-480
		3_flag="non_submerged"
		4_imagestem="castle/castle-to-ice"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_LF"
	[cfg]
		0_terrainlist="(Mm,Hh)"
		1_adjacent="Ai,W*,S*"
		2_layer=-482
		3_flag="non_submerged"
		4_imagestem="hills/regular-to-water"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_LF"
	[cfg]
		0_terrainlist="(Md,Hhd,Mv)"
		1_adjacent="Ai,W*,S*"
		2_layer=-482
		3_flag="non_submerged"
		4_imagestem="hills/dry-to-water"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_LF"
	[cfg]
		0_terrainlist="(R*,G*,Uue)"
		1_adjacent="Ai,W*"
		2_layer=-483
		3_flag="non_submerged"
		4_imagestem="flat/bank-to-ice"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_LF"
	[cfg]
		0_terrainlist="(U*,Xu*,Ql*)"
		1_adjacent="Ai,W*,S*"
		2_layer=-486
		3_flag="non_submerged"
		4_imagestem="cave/bank"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_LF"
	[cfg]
		0_terrainlist="Aa,Ai"
		1_adjacent="(D*)"
		2_layer=-485
		3_flag="non_submerged"
		4_imagestem="frozen/ice"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_LF"
	[cfg]
		0_terrainlist="Aa,Ha,Ms,Ai"
		1_adjacent="(W*,S*)"
		2_layer=-485
		3_flag="non_submerged"
		4_imagestem="frozen/ice"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_LF"
	[cfg]
		0_terrainlist="Aa,Ha,Ms,Ai"
		1_adjacent="(W*,S*)"
		2_layer=-505
		3_flag="submerged"
		4_imagestem="frozen/ice-to-water"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="Aa,Ha,Ms,Ai"
		1_adjacent="(W*,Ss)"
		2_layer=-1001
		3_imagestem="frozen/ice-to-water"
	[/cfg]
[/macro]

[macro]
	type="NEW:BEACH"
	[cfg]
		0_terrainlist="D*,Hd"
		1_adjacent="W*"
		2_imagestem="sand/shore"
	[/cfg]
[/macro]

[macro]
	type="NEW:BEACH"
	[cfg]
		0_terrainlist="!,Chw,Khw,Khs,W*,S*,Xv,Qx*,A*,_*"
		1_adjacent="W*"
		2_imagestem="flat/shore"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_LF"
	[cfg]
		0_terrainlist="Wwf"
		1_adjacent="(!,Wwf,!,W*,Sm,Xv,_off^_usr)"
		2_layer=-515
		3_flag="transition2"
		4_imagestem="water/ford"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_LF"
	[cfg]
		0_terrainlist="Sm"
		1_adjacent="(!,Sm,!,W*,D*)"
		2_layer=-556
		3_flag="transition3"
		4_imagestem="swamp/mud-long"
	[/cfg]
[/macro]

[macro]
	type="ANIMATED_WATER_15_TRANSITION"
	[cfg]
		0_terrainlist="Wo"
		1_adjacent="(!,Wo,!,W*,S*)"
		2_layer=-550
		3_ipf="()"
		4_time=150
		5_imagestem="water/ocean-blend"
	[/cfg]
[/macro]

[macro]
	type="ANIMATED_WATER_15_TRANSITION"
	[cfg]
		0_terrainlist="Wot"
		1_adjacent="(!,Wot,!,W*,S*)"
		2_layer=-551
		3_ipf="~CS(-45,-5,25)"
		4_time=150
		5_imagestem="water/ocean-blend"
	[/cfg]
[/macro]

[macro]
	type="ANIMATED_WATER_15_TRANSITION"
	[cfg]
		0_terrainlist="Wog"
		1_adjacent="(!,Wog,!,W*,S*)"
		2_layer=-552
		3_ipf="~CS(15,0,-30)"
		4_time=150
		5_imagestem="water/ocean-blend"
	[/cfg]
[/macro]

[macro]
	type="ANIMATED_WATER_15_TRANSITION"
	[cfg]
		0_terrainlist="Ww,Wwf,Wwr"
		1_adjacent="(!,Ww,Wwf,Wwr,!,W*,S*)"
		2_layer=-553
		3_ipf="~CS(40,0,-30)"
		4_time=110
		5_imagestem="water/coast-tropical-long"
	[/cfg]
[/macro]

[macro]
	type="ANIMATED_WATER_15_TRANSITION"
	[cfg]
		0_terrainlist="Wwg,Wwrg"
		1_adjacent="(!,Wwg,Wwrg,!,W*,S*)"
		2_layer=-554
		3_ipf="~CS(60,0,-50)"
		4_time=110
		5_imagestem="water/coast-tropical-long"
	[/cfg]
[/macro]

[macro]
	type="ANIMATED_WATER_15_TRANSITION"
	[cfg]
		0_terrainlist="Wwt,Wwrt"
		1_adjacent="(!,Wwt,Wwrt,!,W*,S*)"
		2_layer=-555
		3_ipf="()"
		4_time=110
		5_imagestem="water/coast-tropical-long"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_LB"
	[cfg]
		0_terrainlist="Wo"
		1_adjacent="(Xv,_off^_usr)"
		2_layer=-560
		3_builder="ANIMATION_15_SLOW"
		4_imagestem="water/ocean-long"
	[/cfg]
[/macro]

[macro]
	type="ANIMATED_WATER_15_TRANSITION"
	[cfg]
		0_terrainlist="Wot"
		1_adjacent="(Xv,_off^_usr)"
		2_layer=-561
		3_ipf="~CS(-40,0,30)"
		4_time=150
		5_imagestem="water/ocean-long"
	[/cfg]
[/macro]

[macro]
	type="ANIMATED_WATER_15_TRANSITION"
	[cfg]
		0_terrainlist="Wog"
		1_adjacent="(Xv,_off^_usr)"
		2_layer=-562
		3_ipf="~CS(15,0,-30)"
		4_time=150
		5_imagestem="water/ocean-long"
	[/cfg]
[/macro]

[macro]
	type="ANIMATED_WATER_15_TRANSITION"
	[cfg]
		0_terrainlist="Ww,Wwf,Wwr"
		1_adjacent="(Xv,_off^_usr)"
		2_layer=-553
		3_ipf="~CS(40,0,-30)"
		4_time=110
		5_imagestem="water/coast-tropical"
	[/cfg]
[/macro]

[macro]
	type="ANIMATED_WATER_15_TRANSITION"
	[cfg]
		0_terrainlist="Wwg,Wwrg"
		1_adjacent="(Xv,_off^_usr)"
		2_layer=-554
		3_ipf="~CS(60,0,-50)"
		4_time=110
		5_imagestem="water/coast-tropical"
	[/cfg]
[/macro]

[macro]
	type="ANIMATED_WATER_15_TRANSITION"
	[cfg]
		0_terrainlist="Wwt,Wwrt"
		1_adjacent="(Xv,_off^_usr)"
		2_layer=-555
		3_ipf="()"
		4_time=110
		5_imagestem="water/coast-tropical"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_LF"
	[cfg]
		0_terrainlist="Qxu,Qxe"
		1_adjacent="Qxua,Xv,_off^_usr"
		2_layer=-600
		3_flag="depths"
		4_imagestem="chasm/depths"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_LF"
	[cfg]
		0_terrainlist="Qxua"
		1_adjacent="Xv,_off^_usr"
		2_layer=-601
		3_flag="depths"
		4_imagestem="chasm/abyss-base"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="Ai"
		1_adjacent="Xv,_off^_usr"
		2_layer=-800
		3_imagestem="frozen/ice"
	[/cfg]
[/macro]

[macro]
	type="TRANSITION_COMPLETE_L"
	[cfg]
		0_terrainlist="Xv"
		1_adjacent="_off^_usr"
		2_layer=-810
		3_imagestem="void/void"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE"
	[cfg]
		0_terrainlist="*"
		1_imagestem="void/void"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_SINGLEHEX_LB"
	[cfg]
		0_terrainlist="Ch,Chr,Cha"
		1_layer=0
		2_builder="IMAGE_SINGLE"
		3_imagestem="flat/wall"
	[/cfg]
[/macro]

[macro]
	type="TERRAIN_BASE_SINGLEHEX_LB"
	[cfg]
		0_terrainlist="Kud"
		1_layer=0
		2_builder="IMAGE_SINGLE"
		3_imagestem="castle/keep-floor"
	[/cfg]
[/macro]

[macro]
	type="NONE"
	[cfg]
		map="
	2
	,  3
	1"
		probability=100
		rotations="tr,r,br,bl,l,tl"
		[tile]
			pos=1
			set_no_flag="wall-@R0"
			type="Ch,Cv,Co,Cd,Kud"
		[/tile]
		[tile]
			pos=2
			set_no_flag="wall-@R2"
			type="!,C*,K*"
		[/tile]
		[tile]
			pos=3
			set_no_flag="wall-@R4"
			type="!,C*,K*"
		[/tile]
		[image]
			base="54,72"
			layer=1
			name="castle/castle@V-convex-@R0.png"
			variations=";2;3;4;5;6"
		[/image]
	[/cfg]
[/macro]

[macro]
	type="NONE"
	[cfg]
		map="
	2
	,  3
	1"
		probability=100
		rotations="tr,r,br,bl,l,tl"
		[tile]
			pos=1
			set_no_flag="wall-@R0"
			type="!,C*,K*"
		[/tile]
		[tile]
			pos=2
			set_no_flag="wall-@R2"
			type="Ch,Cv,Co,Cd,Kud"
		[/tile]
		[tile]
			pos=3
			set_no_flag="wall-@R4"
			type="Ch,Cv,Co,Cd,Kud"
		[/tile]
		[image]
			base="54,72"
			layer=1
			name="castle/castle@V-concave-@R0.png"
			variations=";2;3;4;5;6"
		[/image]
	[/cfg]
[/macro]
