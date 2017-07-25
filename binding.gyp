{  
'variables': {
   },
  "targets": [
    {
    
        "target_name": "removeskin",
        "sources": [ 
            'remove-skin.cc'
        ],
       "include_dirs" : [
	    "<!(node -e \"require('nan')\")",
	    "<!(node -e \"require('free-image-lib/include_dirs')\")"
       ],
       "libraries": [
	    "../<!(node -e \"require('free-image-lib/libraries')\")"
       ]
    }
    
  ]
}

