{  
'variables': {
   },
  "targets": [
    {
    
        "target_name": "removeskin",
        "sources": [ 
            'src/remove-skin.cc'
        ],
        'libraries': [
            '-lfreeimage'
        ],
        "include_dirs": [
    	  "<!(node -e \"require('nan')\")"
    	]
    }
    
  ]
}

