#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define MAX_APPLIANCES 100
#define NAME_LENGTH 80
#define DATA_FILE "appliances.dat"
#define REPORT_FILE "energy_report.txt"

typedef struct {
    char name[NAME_LENGTH];
    double power_watts;
    double hours_per_day;
    int days_per_month;
} Appliance;

static int read_line(const char *prompt, char *buffer, size_t buffer_size) {
    size_t length;

    fputs(prompt, stdout);
    fflush(stdout);

    if (fgets(buffer, (int)buffer_size, stdin) == NULL) {
        return 0;
    }

    length = strlen(buffer);
    if (length > 0 && buffer[length - 1] == '\n') {
        buffer[--length] = '\0';
        if (length > 0 && buffer[length - 1] == '\r') {
            buffer[--length] = '\0';
        }
    } else if (!feof(stdin)) {
        int character;
        while ((character = getchar()) != '\n' && character != EOF) {
            /* Discard the rest of an overlong input line. */
        }
        puts("Input is too long; please try again.");
        return -1;
    }

    return 1;
}

static int read_double(const char *prompt, double minimum, double maximum,
                       double *value) {
    char input[128];

    for (;;) {
        char *end;
        double parsed;
        int status = read_line(prompt, input, sizeof(input));
        if (status == 0) {
            return 0;
        }
        if (status < 0) {
            continue;
        }

        errno = 0;
        parsed = strtod(input, &end);
        while (isspace((unsigned char)*end)) {
            ++end;
        }
        if (end != input && *end == '\0' && errno != ERANGE &&
            isfinite(parsed) && parsed >= minimum && parsed <= maximum) {
            *value = parsed;
            return 1;
        }
        printf("Enter a number from %.2f to %.2f.\n", minimum, maximum);
    }
}

static int read_integer(const char *prompt, int minimum, int maximum,
                        int *value) {
    char input[128];

    for (;;) {
        char *end;
        long parsed;
        int status = read_line(prompt, input, sizeof(input));
        if (status == 0) {
            return 0;
        }
        if (status < 0) {
            continue;
        }

        errno = 0;
        parsed = strtol(input, &end, 10);
        while (isspace((unsigned char)*end)) {
            ++end;
        }
        if (end != input && *end == '\0' && errno != ERANGE &&
            parsed >= minimum && parsed <= maximum) {
            *value = (int)parsed;
            return 1;
        }
        printf("Enter a whole number from %d to %d.\n", minimum, maximum);
    }
}

static int valid_appliance(const Appliance *appliance) {
    return appliance->name[0] != '\0' &&
           strchr(appliance->name, '|') == NULL &&
           appliance->power_watts > 0.0 &&
           appliance->power_watts <= 1000000.0 &&
           appliance->hours_per_day > 0.0 &&
           appliance->hours_per_day <= 24.0 &&
           appliance->days_per_month >= 1 &&
           appliance->days_per_month <= 31;
}

static double daily_kwh(const Appliance *appliance) {
    return appliance->power_watts * appliance->hours_per_day / 1000.0;
}

static double monthly_kwh(const Appliance *appliance) {
    return daily_kwh(appliance) * appliance->days_per_month;
}

static double total_monthly_kwh(const Appliance appliances[], size_t count) {
    size_t i;
    double total = 0.0;
    for (i = 0; i < count; ++i) {
        total += monthly_kwh(&appliances[i]);
    }
    return total;
}

/* Illustrative progressive energy-only slabs; edit these for a local tariff. */
static double estimate_bill(double units) {
    static const double upper_limits[] = {100.0, 200.0, 500.0};
    static const double rates[] = {3.0, 5.0, 7.0, 8.0};
    double bill = 0.0;
    double lower_limit = 0.0;
    size_t i;

    if (units < 0.0) {
        units = 0.0;
    }
    for (i = 0; i < 3; ++i) {
        double upper_limit = upper_limits[i];
        double units_in_slab = units < upper_limit ? units - lower_limit
                                                   : upper_limit - lower_limit;
        if (units_in_slab > 0.0) {
            bill += units_in_slab * rates[i];
        }
        lower_limit = upper_limit;
        if (units <= upper_limit) {
            return bill;
        }
    }
    if (units > lower_limit) {
        bill += (units - lower_limit) * rates[3];
    }
    return bill;
}

static int save_data(const Appliance appliances[], size_t count) {
    FILE *file = fopen(DATA_FILE, "w");
    size_t i;
    int ok = 1;

    if (file == NULL) {
        perror("Could not save appliance data");
        return 0;
    }

    for (i = 0; i < count; ++i) {
        if (fprintf(file, "%s|%.6f|%.6f|%d\n", appliances[i].name,
                    appliances[i].power_watts, appliances[i].hours_per_day,
                    appliances[i].days_per_month) < 0) {
            ok = 0;
            break;
        }
    }
    if (fclose(file) != 0) {
        ok = 0;
    }
    if (!ok) {
        perror("Could not finish saving appliance data");
    }
    return ok;
}

static size_t load_data(Appliance appliances[], size_t capacity) {
    FILE *file = fopen(DATA_FILE, "r");
    char line[256];
    size_t count = 0;
    size_t line_number = 0;

    if (file == NULL) {
        if (errno != ENOENT) {
            perror("Could not open appliance data");
        }
        return 0;
    }

    while (fgets(line, sizeof(line), file) != NULL) {
        Appliance appliance = {{0}, 0.0, 0.0, 0};
        ++line_number;

        if (sscanf(line, "%79[^|]|%lf|%lf|%d", appliance.name,
                   &appliance.power_watts, &appliance.hours_per_day,
                   &appliance.days_per_month) != 4 ||
            !valid_appliance(&appliance)) {
            fprintf(stderr, "Skipping invalid record on line %zu of %s.\n",
                    line_number, DATA_FILE);
            continue;
        }
        if (count >= capacity) {
            fprintf(stderr, "Only the first %d records can be loaded.\n",
                    MAX_APPLIANCES);
            break;
        }
        appliances[count++] = appliance;
    }

    if (ferror(file)) {
        perror("Error while reading appliance data");
    }
    fclose(file);
    return count;
}

static void print_summary(FILE *output, const Appliance appliances[],
                          size_t count) {
    double daily_total = 0.0;
    double monthly_total;
    size_t i;

    if (count == 0) {
        fprintf(output, "No appliance records yet. Add an appliance to begin.\n");
        return;
    }

    for (i = 0; i < count; ++i) {
        daily_total += daily_kwh(&appliances[i]);
    }
    monthly_total = total_monthly_kwh(appliances, count);

    fprintf(output, "Appliances recorded: %zu\n", count);
    fprintf(output, "Daily use when all listed appliances run: %.2f kWh\n",
            daily_total);
    fprintf(output, "Projected monthly use: %.2f kWh\n", monthly_total);
    fprintf(output, "Estimated energy charge: INR %.2f\n",
            estimate_bill(monthly_total));
    fprintf(output, "Estimate uses sample slabs and excludes taxes and fixed fees.\n");
}

static void print_appliances(FILE *output, const Appliance appliances[],
                             size_t count) {
    size_t i;

    if (count == 0) {
        fprintf(output, "No appliance records yet.\n");
        return;
    }

    fprintf(output, "\n%-4s %-24s %12s %12s %10s %12s %13s\n",
            "No.", "Appliance", "Power (W)", "Hours/day", "Days/mo.",
            "Daily kWh", "Monthly kWh");
    fprintf(output, "-----------------------------------------------------------------------------------------------\n");
    for (i = 0; i < count; ++i) {
        fprintf(output, "%-4zu %-24.24s %12.1f %12.2f %10d %12.2f %13.2f\n",
                i + 1, appliances[i].name, appliances[i].power_watts,
                appliances[i].hours_per_day, appliances[i].days_per_month,
                daily_kwh(&appliances[i]), monthly_kwh(&appliances[i]));
    }
}

static void sort_by_monthly_use(Appliance appliances[], size_t count) {
    size_t end;

    for (end = count; end > 1; --end) {
        size_t i;
        int swapped = 0;
        for (i = 0; i + 1 < end; ++i) {
            Appliance *left = &appliances[i];
            Appliance *right = &appliances[i + 1];
            if (monthly_kwh(left) < monthly_kwh(right)) {
                Appliance temporary = *left;
                *left = *right;
                *right = temporary;
                swapped = 1;
            }
        }
        if (!swapped) {
            break;
        }
    }
}

static void print_analysis(FILE *output, const Appliance appliances[],
                           size_t count) {
    Appliance ranked[MAX_APPLIANCES];
    double total = total_monthly_kwh(appliances, count);
    size_t i;

    if (count == 0) {
        fprintf(output, "No usage to analyze yet.\n");
        return;
    }

    memcpy(ranked, appliances, count * sizeof(Appliance));
    sort_by_monthly_use(ranked, count);

    fprintf(output, "\nMonthly consumption ranking (highest first)\n");
    fprintf(output, "%-5s %-28s %15s %12s\n", "Rank", "Appliance",
            "Monthly kWh", "Share");
    for (i = 0; i < count; ++i) {
        double use = monthly_kwh(&ranked[i]);
        double share = total > 0.0 ? use * 100.0 / total : 0.0;
        fprintf(output, "%-5zu %-28.28s %15.2f %11.1f%%\n", i + 1,
                ranked[i].name, use, share);
    }
}

static void print_recommendations(FILE *output, const Appliance appliances[],
                                  size_t count) {
    Appliance ranked[MAX_APPLIANCES];
    double total = total_monthly_kwh(appliances, count);
    double potential_savings = 0.0;
    size_t i;

    if (count == 0) {
        fprintf(output, "No usage data available for suggestions.\n");
        return;
    }

    memcpy(ranked, appliances, count * sizeof(Appliance));
    sort_by_monthly_use(ranked, count);
    fprintf(output, "\nUsage optimization suggestions\n");

    for (i = 0; i < count; ++i) {
        double use = monthly_kwh(&ranked[i]);
        double share = total > 0.0 ? use / total : 0.0;
        double savings;

        if (ranked[i].hours_per_day > 1.0) {
            savings = ranked[i].power_watts / 1000.0 *
                      (ranked[i].hours_per_day < 1.0
                           ? ranked[i].hours_per_day
                           : 1.0) *
                      ranked[i].days_per_month;
            fprintf(output,
                    "- %s: try reducing use by one hour on days it runs; "
                    "estimated saving %.2f kWh/month.\n",
                    ranked[i].name, savings);
        } else {
            savings = use * 0.20;
            fprintf(output,
                    "- %s: compare efficient models; a 20%% lower-power "
                    "replacement could save about %.2f kWh/month.\n",
                    ranked[i].name, savings);
        }

        if (share >= 0.20 || use >= 30.0) {
            fprintf(output,
                    "  This is a high-use item in the current record set "
                    "(%.1f%% of projected consumption).\n",
                    share * 100.0);
        }
        potential_savings += savings;
    }

    if (potential_savings > total) {
        potential_savings = total;
    }
    fprintf(output, "Combined example savings: up to %.2f kWh/month "
                    "(about INR %.2f in energy charges).\n",
            potential_savings,
            estimate_bill(total) - estimate_bill(total - potential_savings));
    fprintf(output,
            "Savings are estimates; actual results depend on real usage and "
            "appliance performance.\n");
}

static void add_appliance(Appliance appliances[], size_t *count) {
    char input[256];
    Appliance appliance = {{0}, 0.0, 0.0, 0};
    double value;
    char *start;
    char *end;
    int status;

    if (*count >= MAX_APPLIANCES) {
        printf("The project supports up to %d appliances.\n", MAX_APPLIANCES);
        return;
    }

    for (;;) {
        status = read_line("Appliance name: ", input, sizeof(input));
        if (status == 0) {
            return;
        }
        if (status < 0) {
            continue;
        }
        start = input;
        while (isspace((unsigned char)*start)) {
            ++start;
        }
        end = start + strlen(start);
        while (end > start && isspace((unsigned char)end[-1])) {
            *--end = '\0';
        }
        if (start != input) {
            memmove(input, start, strlen(start) + 1);
        }
        if (input[0] == '\0' || strchr(input, '|') != NULL) {
            puts("Enter a name and do not use the | character.");
            continue;
        }
        if (strlen(input) >= NAME_LENGTH) {
            printf("Use a name shorter than %d characters.\n", NAME_LENGTH);
            continue;
        }
        strcpy(appliance.name, input);
        break;
    }

    if (!read_double("Power rating in watts (0-1000000): ", 0.01, 1000000.0,
                     &value)) {
        return;
    }
    appliance.power_watts = value;
    if (!read_double("Hours used per day (0-24): ", 0.01, 24.0, &value)) {
        return;
    }
    appliance.hours_per_day = value;
    if (!read_integer("Days used per month (1-31): ", 1, 31,
                      &appliance.days_per_month)) {
        return;
    }

    if (!valid_appliance(&appliance)) {
        puts("That appliance record is not valid.");
        return;
    }

    appliances[(*count)++] = appliance;
    if (save_data(appliances, *count)) {
        puts("Appliance added and saved.");
    } else {
        puts("Appliance added for this run, but could not be saved.");
    }
}

static void remove_appliance(Appliance appliances[], size_t *count) {
    int selection;
    size_t index;

    if (*count == 0) {
        puts("There are no appliance records to remove.");
        return;
    }

    print_appliances(stdout, appliances, *count);
    if (!read_integer("Record number to remove: ", 1, (int)*count,
                      &selection)) {
        return;
    }
    index = (size_t)(selection - 1);
    if (index + 1 < *count) {
        memmove(&appliances[index], &appliances[index + 1],
                (*count - index - 1) * sizeof(Appliance));
    }
    --(*count);

    if (save_data(appliances, *count)) {
        puts("Appliance removed and records saved.");
    } else {
        puts("Appliance removed for this run, but the file could not be updated.");
    }
}

static void generate_report(const Appliance appliances[], size_t count) {
    FILE *file = fopen(REPORT_FILE, "w");
    time_t now;
    struct tm *local_time;
    char timestamp[64] = "Date unavailable";

    if (file == NULL) {
        perror("Could not create report");
        return;
    }

    now = time(NULL);
    local_time = localtime(&now);
    if (local_time != NULL) {
        (void)strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M",
                       local_time);
    }

    fprintf(file, "Smart Energy Consumption Analysis and Optimization System\n");
    fprintf(file, "Report generated: %s\n\n", timestamp);
    print_summary(file, appliances, count);
    print_appliances(file, appliances, count);
    print_analysis(file, appliances, count);
    print_recommendations(file, appliances, count);

    if (fclose(file) != 0) {
        perror("Could not finish writing report");
        return;
    }
    printf("Report saved to %s.\n", REPORT_FILE);
}

static void print_menu(void) {
    puts("\nSmart Energy Consumption Analysis and Optimization System");
    puts("1. Add appliance");
    puts("2. View appliances and usage");
    puts("3. Analyze and rank consumption");
    puts("4. Show optimization suggestions");
    puts("5. Generate text report");
    puts("6. Remove appliance");
    puts("7. Save and exit");
}

int main(void) {
    Appliance appliances[MAX_APPLIANCES];
    size_t count = load_data(appliances, MAX_APPLIANCES);
    int choice;

    if (count > 0) {
        printf("Loaded %zu appliance record(s) from %s.\n", count, DATA_FILE);
    } else {
        printf("No saved appliance records found. Data will be stored in %s.\n",
               DATA_FILE);
    }

    for (;;) {
        print_menu();
        if (!read_integer("Choose an option (1-7): ", 1, 7, &choice)) {
            puts("\nInput ended. Saving records before exit.");
            break;
        }

        switch (choice) {
            case 1:
                add_appliance(appliances, &count);
                break;
            case 2:
                print_summary(stdout, appliances, count);
                print_appliances(stdout, appliances, count);
                break;
            case 3:
                print_summary(stdout, appliances, count);
                print_analysis(stdout, appliances, count);
                break;
            case 4:
                print_recommendations(stdout, appliances, count);
                break;
            case 5:
                generate_report(appliances, count);
                break;
            case 6:
                remove_appliance(appliances, &count);
                break;
            case 7:
                if (save_data(appliances, count)) {
                    puts("Records saved. Goodbye.");
                } else {
                    puts("Could not save records. Goodbye.");
                }
                return 0;
            default:
                break;
        }
    }

    (void)save_data(appliances, count);
    return 0;
}

