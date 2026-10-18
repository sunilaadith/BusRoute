#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct {
	char **stops;
	size_t size;
	size_t capacity;
} Route;

typedef enum {
	ADD_STOP,
	REMOVE_STOP
} ActionType;

typedef struct {
	ActionType type;
	char *stop;
	size_t index;
} Action;

typedef struct {
	Action *actions;
	size_t size;
	size_t capacity;
} ActionStack;

static char *copy_string(const char *text)
{
	size_t length = strlen(text) + 1;
	char *copy = malloc(length);

	if (copy != NULL) {
		memcpy(copy, text, length);
	}
	return copy;
}

static int reserve_route(Route *route, size_t needed)
{
	if (needed <= route->capacity) {
		return 1;
	}

	size_t capacity = route->capacity == 0 ? 4 : route->capacity;
	while (capacity < needed) {
		capacity *= 2;
	}

	char **stops = realloc(route->stops, capacity * sizeof(*stops));
	if (stops == NULL) {
		return 0;
	}

	route->stops = stops;
	route->capacity = capacity;
	return 1;
}

static int insert_stop(Route *route, size_t index, const char *stop)
{
	if (index > route->size || !reserve_route(route, route->size + 1)) {
		return 0;
	}

	char *copy = copy_string(stop);
	if (copy == NULL) {
		return 0;
	}

	memmove(&route->stops[index + 1], &route->stops[index],
			(route->size - index) * sizeof(*route->stops));
	route->stops[index] = copy;
	route->size++;
	return 1;
}

static char *remove_stop(Route *route, size_t index)
{
	char *stop = route->stops[index];
	memmove(&route->stops[index], &route->stops[index + 1],
			(route->size - index - 1) * sizeof(*route->stops));
	route->size--;
	return stop;
}

static int push_action(ActionStack *stack, Action action)
{
	if (stack->size == stack->capacity) {
		size_t capacity = stack->capacity == 0 ? 4 : stack->capacity * 2;
		Action *actions = realloc(stack->actions, capacity * sizeof(*actions));
		if (actions == NULL) {
			return 0;
		}
		stack->actions = actions;
		stack->capacity = capacity;
	}

	stack->actions[stack->size++] = action;
	return 1;
}

static Action pop_action(ActionStack *stack)
{
	return stack->actions[--stack->size];
}

static void clear_stack(ActionStack *stack)
{
	while (stack->size > 0) {
		free(pop_action(stack).stop);
	}
}

static int read_line(char *buffer, size_t capacity)
{
	if (fgets(buffer, (int)capacity, stdin) == NULL) {
		return 0;
	}

	size_t length = strlen(buffer);
	if (length > 0 && buffer[length - 1] == '\n') {
		buffer[length - 1] = '\0';
	} else {
		int character;
		while ((character = getchar()) != '\n' && character != EOF) {
		}
	}
	return 1;
}

static int read_choice(void)
{
	char line[64];
	char *end;
	long choice;

	if (!read_line(line, sizeof(line))) {
		return 0;
	}

	choice = strtol(line, &end, 10);
	if (end == line || *end != '\0' || choice < 0 || choice > 6) {
		return -1;
	}
	return (int)choice;
}

static void print_route(const Route *route)
{
	if (route->size == 0) {
		puts("Route is empty.");
		return;
	}

	puts("Route:");
	for (size_t index = 0; index < route->size; index++) {
		printf("%zu. %s\n", index + 1, route->stops[index]);
	}
}

int main(void)
{
	Route route = {NULL, 0, 0};
	ActionStack undo_stack = {NULL, 0, 0};
	ActionStack redo_stack = {NULL, 0, 0};
	char input[256];
	int running = 1;

	while (running) {
		puts("\n1. Add stop\n2. Remove stop\n3. Print route\n"
			 "4. Undo\n5. Redo\n6. Exit");
		printf("Choice: ");

		int choice = read_choice();
		if (choice == 0) {
			break;
		}
		if (choice < 0) {
			puts("Invalid choice.");
			continue;
		}

		if (choice == 1) {
			printf("Stop name: ");
			if (!read_line(input, sizeof(input))) {
				break;
			}
			if (input[0] == '\0') {
				puts("Stop name cannot be empty.");
				continue;
			}

			Action action = {ADD_STOP, copy_string(input), route.size};
			if (action.stop == NULL || !insert_stop(&route, route.size, input)) {
				free(action.stop);
				puts("Unable to add stop: not enough memory.");
				continue;
			}
			if (!push_action(&undo_stack, action)) {
				free(remove_stop(&route, route.size - 1));
				free(action.stop);
				puts("Unable to record operation: not enough memory.");
				continue;
			}
			clear_stack(&redo_stack);
			printf("Added %s.\n", input);
		} else if (choice == 2) {
			if (route.size == 0) {
				puts("There are no stops to remove.");
				continue;
			}

			print_route(&route);
			printf("Stop number to remove: ");
			if (!read_line(input, sizeof(input))) {
				break;
			}
			char *end;
			long number = strtol(input, &end, 10);
			if (end == input || *end != '\0' || number < 1 ||
				(size_t)number > route.size) {
				puts("Invalid stop number.");
				continue;
			}

			size_t index = (size_t)number - 1;
			Action action = {REMOVE_STOP, copy_string(route.stops[index]), index};
			if (action.stop == NULL) {
				puts("Unable to record operation: not enough memory.");
				continue;
			}
			char *removed = remove_stop(&route, index);
			free(removed);
			if (!push_action(&undo_stack, action)) {
				if (!insert_stop(&route, index, action.stop)) {
					free(action.stop);
					puts("Unable to restore stop: not enough memory.");
					continue;
				}
				free(action.stop);
				puts("Unable to record operation: not enough memory.");
				continue;
			}
			clear_stack(&redo_stack);
			printf("Removed %s.\n", action.stop);
		} else if (choice == 3) {
			print_route(&route);
		} else if (choice == 4 || choice == 5) {
			ActionStack *source = choice == 4 ? &undo_stack : &redo_stack;
			ActionStack *destination = choice == 4 ? &redo_stack : &undo_stack;
			if (source->size == 0) {
				puts(choice == 4 ? "Nothing to undo." : "Nothing to redo.");
				continue;
			}

			Action action = source->actions[source->size - 1];
			if (!push_action(destination, action)) {
				puts("Unable to record operation: not enough memory.");
				continue;
			}

			if (choice == 4) {
				if (action.type == ADD_STOP) {
					free(remove_stop(&route, action.index));
				} else if (!insert_stop(&route, action.index, action.stop)) {
					destination->size--;
					puts("Unable to undo: not enough memory.");
					continue;
				}
				puts("Undone.");
			} else {
				if (action.type == ADD_STOP) {
					if (!insert_stop(&route, action.index, action.stop)) {
						destination->size--;
						puts("Unable to redo: not enough memory.");
						continue;
					}
				} else {
					free(remove_stop(&route, action.index));
				}
				puts("Redone.");
			}
			source->size--;
		} else {
			running = 0;
		}
	}

	for (size_t index = 0; index < route.size; index++) {
		free(route.stops[index]);
	}
	free(route.stops);
	clear_stack(&undo_stack);
	clear_stack(&redo_stack);
	free(undo_stack.actions);
	free(redo_stack.actions);
	return 0;
}

