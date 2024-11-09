{
  "targets": [
    {
      "target_name": "WiredTiger",
      "sources": [
        "src/wired_tiger.c",
        "src/binding/helpers.cc",
        "src/binding/find_cursor.cc",
        "src/binding/session.cc",
        "src/binding/cursor.cc",
        "src/binding/table.cc",
        "src/binding/map_table.cc",
        "src/binding/db.cc",
        "src/binding/custom_extractor.cc",
        "src/binding/custom_collator.cc",
        "src/binding/cfg_item.cc",
        "src/binding/map.cc",
        "src/wiredtiger/helpers.cc",
        "src/wiredtiger/find_cursor.cc",
        "src/wiredtiger/multi_cursor.cc",
        "src/wiredtiger/cursor.cc",
        "src/wiredtiger/session.cc",
        "src/wiredtiger/db.cc",
        "src/wiredtiger/custom_extractor.cc",
        "src/wiredtiger/custom_collator.cc",
        "src/wiredtiger/table.cc",
        "src/wiredtiger/map_table.cc",
        "src/binding.cc",
      ],
      "include_dirs" : [
        "<!(node -e \"require('nan')\")"
        ],
			'cflags': ['-pthread'],
      "link_settings": {
        "libraries": [
          '-lwiredtiger', '-lz', '-llz4', '-lsnappy', '-lzstd', '-lc', '-lffcall'
        ],
      }
    }
  ]
}
