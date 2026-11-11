#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#define MAX_STUDENTS 100
#define DATA_FILE "students.txt"
#define BACKUP_FILE "students_backup.txt"
#define SUBJECTS 9
#define MAX_POINTS (SUBJECTS * 8)   /* 8 points is the best level in each subject */

/* The nine Junior School learning areas. To add or remove a subject,
   change SUBJECTS above and these two lists (same order in both). */
const char *SUBJECT_NAMES[SUBJECTS] = {
    "English", "Kiswahili", "Mathematics", "Integrated Science",
    "Agriculture", "Social Studies", "Religious Education",
    "Creative Arts & Sports", "Pre-Technical Studies"
};
const char *SUBJECT_CODES[SUBJECTS] = {
    "ENG", "KIS", "MAT", "SCI", "AGR", "SST", "REL", "CAS", "PTE"
};

struct Student
{
    char name[50];
    char regNo[20];
    float marks[SUBJECTS];   /* one mark (0-100) per subject */
    float total;             /* total of all marks */
    float average;           /* average of all marks */
    int points;              /* CBC points: the sum of the subject levels */
};

struct Student students[MAX_STUDENTS];
int studentCount = 0;

void addStudent();
void displayStudents();
void calculateAverage();
void findHighest();
void findLowest();
void clearInputBuffer();
void saveStudents();
void loadStudents();
void searchStudent();
void deleteStudent();

/* CBC performance levels (KNEC scale used for the KJSEA):
     90-100  EE1  8 points     41-57  ME2  5 points
     75-89   EE2  7 points     31-40  AE1  4 points
     58-74   ME1  6 points     21-30  AE2  3 points
                               11-20  BE1  2 points
                                1-10  BE2  1 point
   No zero is awarded, so a mark of 0 also counts as BE2. */
int getPoints(float mark)
{
    if (mark >= 90) return 8;
    if (mark >= 75) return 7;
    if (mark >= 58) return 6;
    if (mark >= 41) return 5;
    if (mark >= 31) return 4;
    if (mark >= 21) return 3;
    if (mark >= 11) return 2;
    return 1;
}

const char *getLevel(float mark)
{
    switch (getPoints(mark))
    {
        case 8: return "EE1";
        case 7: return "EE2";
        case 6: return "ME1";
        case 5: return "ME2";
        case 4: return "AE1";
        case 3: return "AE2";
        case 2: return "BE1";
        default: return "BE2";
    }
}

/* Works out total, average and CBC points from the subject marks. */
void calculateResults(struct Student *s)
{
    s->total = 0;
    s->points = 0;

    for (int k = 0; k < SUBJECTS; k++)
    {
        s->total += s->marks[k];
        s->points += getPoints(s->marks[k]);
    }

    s->average = s->total / SUBJECTS;
}

void printDetails(const struct Student *s)
{
    printf("Name     : %s\n", s->name);
    printf("Reg No   : %s\n", s->regNo);
    printf("\n");

    for (int k = 0; k < SUBJECTS; k++)
    {
        int pts = getPoints(s->marks[k]);
        printf("%-24s: %6.2f%%  %s (%d %s)\n",
               SUBJECT_NAMES[k], s->marks[k],
               getLevel(s->marks[k]), pts, pts == 1 ? "pt" : "pts");
    }

    printf("\n");
    printf("Points   : %d / %d\n", s->points, MAX_POINTS);
    printf("Average  : %.2f%%\n", s->average);
}

/* 1 if student a ranks above student b: more points wins, then higher total marks */
int ranksAbove(const struct Student *a, const struct Student *b)
{
    if (a->points != b->points) return a->points > b->points;
    return a->total > b->total;
}

/* Clears leftover characters (including the newline) from stdin.
   Call this after every scanf that reads a number, before reading
   any string with %[^\n] or before looping back to the menu. */
void clearInputBuffer()
{
    int c;
    while ((c = getchar()) != '\n' && c != EOF);
}

/* Asks for one mark and keeps asking until it is a number from 0 to 100. */
float readMark(const char *subject)
{
    float mark;
    int r;

    printf("Enter %s mark (0-100): ", subject);
    while ((r = scanf("%f", &mark)) != 1 || mark < 0 || mark > 100)
    {
        if (r == EOF)
        {
            printf("\nInput closed. Exiting.\n");
            exit(1);
        }
        printf("Mark must be a number from 0 to 100, try again: ");
        clearInputBuffer();
    }
    return mark;
}

int main()
{
    int choice;

    loadStudents();   /* bring back the saved students */

    while (1)
    {
        printf("\n========== STUDENT MANAGEMENT SYSTEM =========\n");
        printf("1. Add Student\n");
        printf("2. Display Students\n");
        printf("3. Calculate Average\n");
        printf("4. Find the Highest Performer\n");
        printf("5. Find the Lowest Performer\n");
        printf("6. Search Student by Reg No.\n");
        printf("7. Delete Student\n");
        printf("8. Exit\n");
        printf("Enter choice: ");

        if (scanf("%d", &choice) != 1)
        {
            if (feof(stdin)) return 0;
            printf("Invalid input! Please enter a number.\n");
            clearInputBuffer();
            continue;
        }
        clearInputBuffer(); /* consume leftover newline before any string reads */

        switch (choice)
        {
            case 1:
                addStudent();
                break;
            case 2:
                displayStudents();
                break;
            case 3:
                calculateAverage();
                break;
            case 4:
                findHighest();
                break;
            case 5:
                findLowest();
                break;
            case 6:
                searchStudent();
                break;
            case 7:
                deleteStudent();
                break;
            case 8:
                saveStudents();
                printf("\nExiting program. Goodbye!\n");
                return 0;
            default:
                printf("\nInvalid choice. Please try again.\n");
        }
    }

    return 0;
}

void addStudent()
{
    if (studentCount >= MAX_STUDENTS)
    {
        printf("\nDatabase full! Cannot add more students.\n");
        return;
    }

    struct Student s;

    printf("\n--- Add New Student ---\n");

    printf("Enter name: ");
    scanf(" %49[^\n]", s.name);

    printf("Enter Registration Number: ");
    scanf(" %19s", s.regNo);

    for (int i = 0; i < studentCount; i++)
    {
        if (strcmp(students[i].regNo, s.regNo) == 0)
        {
            printf("A student with that registration number already exists.\n");
            clearInputBuffer();
            return;
        }
    }

    for (int k = 0; k < SUBJECTS; k++)
    {
        s.marks[k] = readMark(SUBJECT_NAMES[k]);
    }

    calculateResults(&s);

    students[studentCount] = s;
    studentCount++;

    saveStudents();   /* save straight away so nothing is lost */
    printf("Student added successfully!\n");
    printf("Result: %d / %d points\n", s.points, MAX_POINTS);
}

void displayStudents()
{
    if (studentCount == 0)
    {
        printf("\nNo student records found.\n");
        return;
    }

    printf("\n");
    printf("%-18s %-10s", "Name", "Reg No.");
    for (int k = 0; k < SUBJECTS; k++)
    {
        printf(" %-4s", SUBJECT_CODES[k]);
    }
    printf(" %-7s %-7s\n", "Points", "Average");

    for (int i = 0; i < 18 + 1 + 10 + SUBJECTS * 5 + 1 + 7 + 1 + 7; i++) printf("-");
    printf("\n");

    for (int i = 0; i < studentCount; i++)
    {
        char pts[16];
        snprintf(pts, sizeof pts, "%d/%d", students[i].points, MAX_POINTS);

        printf("%-18.18s %-10s", students[i].name, students[i].regNo);
        for (int k = 0; k < SUBJECTS; k++)
        {
            printf(" %-4s", getLevel(students[i].marks[k]));
        }
        printf(" %-7s %-7.2f\n", pts, students[i].average);
    }

    for (int i = 0; i < 18 + 1 + 10 + SUBJECTS * 5 + 1 + 7 + 1 + 7; i++) printf("-");
    printf("\n");
    printf("EE1 90-100 (8) | EE2 75-89 (7) | ME1 58-74 (6) | ME2 41-57 (5)\n");
    printf("AE1 31-40  (4) | AE2 21-30 (3) | BE1 11-20 (2) | BE2  1-10 (1)\n");
}

void calculateAverage()
{
    if (studentCount == 0)
    {
        printf("\nNo student records available to calculate average.\n");
        return;
    }

    float totalSum = 0.0f;
    for (int i = 0; i < studentCount; i++)
    {
        totalSum += students[i].average;
    }

    float classAverage = totalSum / (float)studentCount;

    printf("\n--- Class Performance ---\n");
    printf("Total Students : %d\n", studentCount);
    printf("Class Average  : %.2f%% (%s)\n", classAverage, getLevel(classAverage));
}

void findHighest()
{
    if (studentCount == 0)
    {
        printf("\nNo records found.\n");
        return;
    }

    int topIndex = 0;
    for (int i = 1; i < studentCount; i++)
    {
        if (ranksAbove(&students[i], &students[topIndex]))
        {
            topIndex = i;
        }
    }

    printf("\n--- Top Performing Student ---\n");
    printDetails(&students[topIndex]);
}

void findLowest()
{
    if (studentCount == 0)
    {
        printf("\nNo records found.\n");
        return;
    }

    int lowIndex = 0;
    for (int i = 1; i < studentCount; i++)
    {
        if (ranksAbove(&students[lowIndex], &students[i]))
        {
            lowIndex = i;
        }
    }

    printf("\n--- Lowest Performing Student ---\n");
    printDetails(&students[lowIndex]);
}

/* Writes every student to DATA_FILE, two lines each:
       Mary Anne
       REG001 80.00 70.00 90.00 ... (one mark per subject)
   The name gets its own line because names can contain spaces.
   Only the marks are saved; levels and points are worked out again on load. */
void saveStudents()
{
    FILE *f = fopen(DATA_FILE, "w");
    if (f == NULL)
    {
        printf("Could not save to %s\n", DATA_FILE);
        return;
    }

    for (int i = 0; i < studentCount; i++)
    {
        fprintf(f, "%s\n%s", students[i].name, students[i].regNo);
        for (int k = 0; k < SUBJECTS; k++)
        {
            fprintf(f, " %.2f", students[i].marks[k]);
        }
        fprintf(f, "\n");
    }

    fclose(f);
}

/* Reads DATA_FILE back into the students array.
   If a record does not match the current subjects (for example a file saved
   by an older version with fewer subjects), the file is kept safe as
   BACKUP_FILE instead of being overwritten later. */
void loadStudents()
{
    FILE *f = fopen(DATA_FILE, "r");
    if (f == NULL) return;   /* no file yet: first run, nothing to load */

    struct Student s;
    char line[100];
    int damaged = 0;

    while (studentCount < MAX_STUDENTS && fgets(line, sizeof line, f) != NULL)
    {
        line[strcspn(line, "\n")] = '\0';          /* remove the newline */
        strncpy(s.name, line, sizeof s.name - 1);
        s.name[sizeof s.name - 1] = '\0';

        int ok = (fscanf(f, "%19s", s.regNo) == 1);
        for (int k = 0; ok && k < SUBJECTS; k++)
        {
            ok = (fscanf(f, "%f", &s.marks[k]) == 1);
        }
        if (!ok)
        {
            damaged = 1;
            break;   /* record does not fit: stop reading */
        }

        /* skip the rest of this record's line */
        int c;
        while ((c = fgetc(f)) != '\n' && c != EOF);

        calculateResults(&s);

        students[studentCount] = s;
        studentCount++;
    }

    fclose(f);

    if (damaged)
    {
        if (rename(DATA_FILE, BACKUP_FILE) == 0)
        {
            printf("Some records in %s did not match the %d subjects.\n", DATA_FILE, SUBJECTS);
            printf("The old file was kept as %s.\n", BACKUP_FILE);
        }
    }

    printf("Loaded %d student(s) from %s\n", studentCount, DATA_FILE);
}

/* Looks for a student by registration number and prints their record. */
void searchStudent()
{
    char regNo[20];

    printf("\nEnter Registration Number to search: ");
    scanf(" %19s", regNo);
    clearInputBuffer();

    for (int i = 0; i < studentCount; i++)
    {
        if (strcmp(students[i].regNo, regNo) == 0)
        {
            printf("\n--- Student Found ---\n");
            printDetails(&students[i]);
            return;
        }
    }

    printf("\nNo student with registration number %s.\n", regNo);
}

/* Removes a student by registration number.
   Everyone after them moves one place left to close the gap. */
void deleteStudent()
{
    char regNo[20];

    printf("\nEnter Registration Number to delete: ");
    scanf(" %19s", regNo);
    clearInputBuffer();

    for (int i = 0; i < studentCount; i++)
    {
        if (strcmp(students[i].regNo, regNo) == 0)
        {
            for (int j = i; j < studentCount - 1; j++)
            {
                students[j] = students[j + 1];
            }
            studentCount--;

            saveStudents();
            printf("\nStudent %s deleted.\n", regNo);
            return;
        }
    }

    printf("\nNo student with registration number %s.\n", regNo);
}
