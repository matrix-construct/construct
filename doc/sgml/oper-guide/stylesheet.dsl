<!DOCTYPE style-sheet PUBLIC "-//James Clark//DTD DSSSL Style Sheet//EN" [
<!ENTITY docbook-html.dsl PUBLIC "-//Norman Walsh//DOCUMENT DocBook HTML Stylesheet//EN" CDATA DSSSL>
<!ENTITY docbook-print.dsl PUBLIC "-//Norman Walsh//DOCUMENT DocBook Print Stylesheet//EN" CDATA DSSSL>
]>

<style-sheet>
<style-specification id="print" use="print-stylesheet">
<style-specification-body> 

(define %generate-book-titlepage% #t)
(define %generate-book-titlepage-on-separate-page% #t)
(define %generate-book-toc% #t)
(define %generate-book-toc-on-titlepage% #f)

</style-specification-body>
</style-specification>

<style-specification id="html" use="html-stylesheet">
<style-specification-body> 

(define %header-navigation% #t)
(define %section-autolabel% #t)
(define %root-filename% "index")
(define %use-id-as-filename% #t)
(define %css-decoration% #t)
(define %example-rules% #t)

</style-specification-body>
</style-specification>

<external-specification id="print-stylesheet" document="docbook-print.dsl">
<external-specification id="html-stylesheet"  document="docbook-html.dsl">
</style-sheet>
