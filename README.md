# C Projects

Two small projects written in C while learning network programming
from Beej's Guide.

## 1. Tiny web server (`webserver.c`)

A web server that runs on port 8080 and serves files from its own folder.

Features:
- Serves any file by path, with the correct `Content-Type` (HTML, CSS, images)
- Handles many browsers at once using `fork()`
- `GET` and `HEAD` requests; `405` for other methods
- `404` for missing files, `403` for paths containing `..`
- Folders serve their `index.html`, and `/docs` redirects to `/docs/`
- Request log written to `server.log`
- Query strings, plus a form example: `form.html` sends a name to `/hello`,
  and the server builds the reply page (input is URL-decoded and HTML-escaped)

Run it:

    gcc webserver.c -o webserver
    ./webserver

Then open http://localhost:8080 in a browser.

## 2. Student management system (`student_management.c`)

A terminal program for recording student results, using the Kenyan
Competency-Based Curriculum (CBC) performance levels for Junior School:
EE1 to BE2, with 1 to 8 points per learning area (maximum 72 points
across nine learning areas).

Features:
- Add, display, search and delete students
- Class average, highest and lowest performer
- Results saved to `students.txt` and loaded again on start

Run it:

    gcc student_management.c -o student_management
    ./student_management

## Notes

`students.txt` and `server.log` are not included in the repository
because they can contain personal information.
